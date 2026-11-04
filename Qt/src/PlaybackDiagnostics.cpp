#include "PlaybackDiagnostics.h"
#include <QStringList>
#include <cmath>

void PlaybackDiagnostics::start() {
    ++m_session;
    m_events = {};
    m_omitted = 0;
    m_clock.start();
    m_recording = true;
}

void PlaybackDiagnostics::clear() {
    m_recording = false;
    m_events = {};
    m_omitted = 0;
    ++m_session;
}

void PlaybackDiagnostics::stop() {
    m_recording = false;
}

QJsonObject PlaybackDiagnostics::numericFields(const QJsonObject &values) {
    QJsonObject safe;
    for (const auto *key : {"sourceSerial",
                            "mediaStatus",
                            "playbackState",
                            "errorCode",
                            "backendFailureCategory",
                            "reportedHttpStatus",
                            "playing",
                            "latchedError",
                            "pendingFailure",
                            "recoverable",
                            "mediaHttpStatus",
                            "mediaNetworkError",
                            "mediaTransferBytes",
                            "mediaExpectedBytes",
                            "mediaReconnects",
                            "mediaTransferActive",
                            "mediaTransferFailed",
                            "hasActiveSource",
                            "controllerFailed",
                            "controllerLoading",
                            "controllerPlaying",
                            "positionMs",
                            "durationMs",
                            "seekable",
                            "bufferProgress",
                            "requested",
                            "volume",
                            "muted",
                            "endedSerial",
                            "recoveryPositionMs",
                            "outputUnavailable",
                            "outputCount",
                            "outputPreferredSampleRateHz",
                            "outputPreferredChannels",
                            "outputPreferredSampleFormat",
                            "outputChannelConfig",
                            "outputChanged",
                            "sampleRateHz",
                            "channels",
                            "sampleFormat",
                            "decodedBuffers",
                            "decodedBytes",
                            "decodedFrames",
                            "lastDecodedElapsedMs",
                            "lastDecodedEndMs",
                            "codec",
                            "bitrate",
                            "sourceKind",
                            "provider",
                            "action",
                            "elapsedMs",
                            "serviceCode",
                            "upstreamHttpStatus",
                            "upstreamCode",
                            "transportFailed",
                            "stale",
                            "requestSerial",
                            "operation",
                            "queueMs",
                            "workerMs",
                            "requestDurationMs",
                            "workerOffsetMs",
                            "replyBytes",
                            "networkError",
                            "httpPhase",
                            "omittedUpstreamEvents",
                            "bridgeState",
                            "bridgeError",
                            "networkReachability",
                            "networkTransport",
                            "networkMetered",
                            "networkCaptive"}) {
        const auto value = values[key];
        if (value.isBool() || (value.isDouble() && std::isfinite(value.toDouble()))) {
            safe[key] = value;
        }
    }
    return safe;
}

void PlaybackDiagnostics::record(const QString &event, const QJsonObject &values) {
    if (!m_recording) {
        return;
    }
    static const QStringList events{"snapshot",     "load",           "load-rejected", "control",
                                    "media-status", "playback-state", "error",         "buffer",
                                    "metadata",     "output",         "request-start", "request-finish",
                                    "upstream",     "stalled",        "media-transfer"};
    if (!events.contains(event)) {
        return;
    }
    if (m_events.size() >= 8192) {
        ++m_omitted;
        return;
    }
    auto safe = numericFields(values);
    safe["elapsedMs"] = double(m_clock.elapsed());
    safe["event"] = event;
    m_events.append(safe);
}

QJsonObject PlaybackDiagnostics::report() const {
    return {
        {"events", m_events},
        {"omittedEvents", m_omitted},
        {"mediaHttpStatusAvailable", true},
        {"upstreamHttpCoverage", "shared-axios-and-global-fetch; other SDK transports may be unavailable"},
        {"upstreamEventLimitPerRequest", 64},
        {"networkPacketLossAvailable", false},
        {"networkDnsTlsTimingAvailable", false},
        {"mediaTransferBytesAvailable", true},
        {"deviceOutputSamplesAvailable", false},
        {"notes", "Relative times only. Request success is not media transfer success. Decoded frames "
                  "do not prove audible device output. No raw errors, URLs, hosts, IPs, headers, "
                  "payloads, account, track, file paths, device names or device IDs are collected."},
        {"codes",
         QJsonObject{
             {"mediaStatus", "QMediaPlayer::MediaStatus"},
             {"playbackState", "QMediaPlayer::PlaybackState"},
             {"errorCode", "QMediaPlayer::Error"},
             {"backendFailureCategory", "Heuristic:0=unknown,1=timeout,2=dns,3=reset,4=invalid-data,5=codec,"
                                        "6=audio-device,7=permission,8=tls,9=eof,10=not-found,11=http"},
             {"reportedHttpStatus", "0=unavailable; otherwise extracted from an explicit backend HTTP error"},
             {"mediaHttpStatus", "HTTP status from managed media stream; 0=unavailable"},
             {"mediaNetworkError", "QNetworkReply::NetworkError"},
             {"sampleFormat", "QAudioFormat::SampleFormat"},
             {"bridgeState", "QProcess::ProcessState"},
             {"bridgeError", "QProcess::ProcessError; may reflect the last error"},
             {"stale", "credential epoch mismatch; playback request validity is managed separately"},
             {"provider", "0=other,1=netease,2=qq,3=kugou,4=local,5=siren,6=apple"},
             {"sourceKind", "0=other,1=https,2=http,3=local-file"},
             {"action", "0=other,1=pause,2=resume,3=stop,4=seek,5=volume"},
             {"operation", "1=playback,2=preview,3=media-url,4=other-playback-upstream,5=upstream-http"},
             {"codec", "QMediaFormat::AudioCodec; -1=unknown"},
             {"httpPhase", "1=response-headers-only(fetch),2=response-complete(axios)"},
             {"networkError", "0=other,1=timeout,2=reset,3=dns-not-found,4=dns-retry,5=refused,6=network-"
                              "unreachable,7=host-unreachable,8=canceled"},
             {"networkReachability", "QNetworkInformation::Reachability; absent=unavailable"},
             {"networkTransport", "QNetworkInformation::TransportMedium; absent=unavailable"}}}};
}
