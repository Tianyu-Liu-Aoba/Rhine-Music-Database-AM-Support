'use strict';
// Apple Music 网页会话桥：用系统 Edge 的 DevTools 协议驱动 music.apple.com，
// 在页面内调用 MusicKit 读取已登录用户的资料库。凭据只存在于 Edge 的独立 profile 里。
const { spawn } = require('node:child_process');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

const PORT = 9223;
const EDGE_CANDIDATES = [
    'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe',
    'C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe',
];
const PROFILE = path.join(os.homedir(), 'AppData', 'Local', 'RhineMusicDatabase', 'am-web-profile');
const LOG = path.join(os.homedir(), 'AppData', 'Local', 'RhineMusicDatabase', 'am-web.log');

function log(message) {
    const line = `[${new Date().toISOString()}] ${message}`;
    try {
        fs.mkdirSync(path.dirname(LOG), { recursive: true });
        fs.appendFileSync(LOG, line + '\n');
    } catch {
        /* 日志失败不影响主流程 */
    }
}

function edgePath() {
    return EDGE_CANDIDATES.find((candidate) => fs.existsSync(candidate)) || null;
}

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

async function targets() {
    const response = await fetch(`http://127.0.0.1:${PORT}/json`);
    return response.json();
}

async function findPage() {
    for (let attempt = 0; attempt < 40; attempt += 1) {
        try {
            const list = await targets();
            const page = list.find((t) => t.type === 'page' && t.url.includes('music.apple.com'));
            if (page) {
                return page;
            }
        } catch {
            /* 还没起来 */
        }
        await sleep(500);
    }
    return null;
}

function launch() {
    const edge = edgePath();
    if (!edge) {
        throw new Error('未找到 Microsoft Edge');
    }
    fs.mkdirSync(PROFILE, { recursive: true });
    const child = spawn(
        edge,
        [
            `--remote-debugging-port=${PORT}`,
            `--user-data-dir=${PROFILE}`,
            '--no-first-run',
            '--no-default-browser-check',
            '--app=https://music.apple.com/browse',
        ],
        { detached: true, stdio: 'ignore' },
    );
    child.unref();
    log('edge launched');
}

async function connect() {
    let page = await findPage();
    if (!page) {
        launch();
        page = await findPage();
    }
    if (!page) {
        throw new Error('无法连接到 Apple Music 网页会话');
    }
    const ws = new WebSocket(page.webSocketDebuggerUrl);
    await new Promise((resolve, reject) => {
        ws.onopen = resolve;
        ws.onerror = reject;
    });
    let nextId = 0;
    const pending = new Map();
    ws.onmessage = (event) => {
        const message = JSON.parse(event.data);
        if (message.id && pending.has(message.id)) {
            pending.get(message.id)(message);
            pending.delete(message.id);
        }
    };
    const send = (method, params) =>
        new Promise((resolve) => {
            const id = ++nextId;
            pending.set(id, resolve);
            ws.send(JSON.stringify({ id, method, params }));
        });
    const evaluate = async (expression, awaitPromise) => {
        const reply = await send('Runtime.evaluate', {
            expression,
            returnByValue: true,
            awaitPromise: Boolean(awaitPromise),
        });
        if (reply.result && reply.result.exceptionDetails) {
            throw new Error(reply.result.exceptionDetails.text || '页面脚本异常');
        }
        return reply.result && reply.result.result ? reply.result.result.value : undefined;
    };
    return { ws, evaluate };
}

async function withSession(task) {
    const session = await connect();
    try {
        const state = await session.evaluate(
            `(function(){ try { var m = MusicKit.getInstance(); return JSON.stringify({ authorized: !!m.isAuthorized }); } catch (e) { return JSON.stringify({ authorized: false, error: String(e) }); } })()`,
        );
        const parsed = JSON.parse(state || '{}');
        if (!parsed.authorized) {
            throw new Error('Apple Music 网页尚未登录');
        }
        return await task(session);
    } finally {
        session.ws.close();
    }
}

// 统一的资料库读取：path 直接沿用 Apple Music API 的 /v1/me/library/... 路径。
function libraryCall(session, apiPath, params) {
    const expression = `(async function () {
        try {
            var res = await MusicKit.getInstance().api.music(${JSON.stringify(apiPath)}, ${JSON.stringify(params || {})});
            var body = res.json || res.data || {};
            var data = body.data || [];
            return JSON.stringify({
                ok: true,
                next: body.next || null,
                items: data.map(function (item) {
                    var a = item.attributes || {};
                    return {
                        id: item.id,
                        type: item.type,
                        name: a.name,
                        artist: a.artistName,
                        album: a.albumName,
                        duration: a.durationInMillis || 0,
                        artwork: a.artwork && a.artwork.url ? a.artwork.url : '',
                        disc: a.discNumber || 0,
                        track: a.trackNumber || 0
                    };
                })
            });
        } catch (e) {
            return JSON.stringify({ ok: false, error: String(e) });
        }
    })()`;
    return session.evaluate(expression, true);
}

async function librarySongs(limit, offset) {
    return withSession(async (session) => {
        const params = { limit: limit || 100 };
        if (offset) {
            params.offset = offset;
        }
        const raw = await libraryCall(session, '/v1/me/library/songs', params);
        return JSON.parse(raw || '{"ok":false}');
    });
}

async function libraryPlaylists() {
    return withSession(async (session) => {
        const raw = await libraryCall(session, '/v1/me/library/playlists', { limit: 50 });
        return JSON.parse(raw || '{"ok":false}');
    });
}

async function libraryAlbums(limit, offset) {
    return withSession(async (session) => {
        const params = { limit: limit || 100 };
        if (offset) {
            params.offset = offset;
        }
        const raw = await libraryCall(session, '/v1/me/library/albums', params);
        return JSON.parse(raw || '{"ok":false}');
    });
}

module.exports = { librarySongs, libraryPlaylists, libraryAlbums, edgePath, PROFILE };

if (require.main === module) {
    const job = (process.argv[2] || 'songs') === 'playlists' ? libraryPlaylists() : librarySongs(10, 0);
    job
        .then((result) => {
            process.stdout.write(JSON.stringify(result, null, 2) + '\n');
            process.exit(result.ok ? 0 : 2);
        })
        .catch((error) => {
            process.stderr.write(String(error && error.message) + '\n');
            process.exit(1);
        });
}
