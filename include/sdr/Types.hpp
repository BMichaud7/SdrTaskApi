#pragma once
/**
 * @file Types.hpp
 * @brief Core type system shared across the SDR stack.
 *
 * All message structs, enumerations, and helper functions that cross the
 * SdrResourceManager / AcquisitionApp / AnalysisApp boundary are defined
 * here.  Consumers include this header via the `sdr/` include path.
 */
#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <chrono>
#include <limits>
#include <functional>
#include <atomic>

/// @namespace sdr
/// @brief Top-level namespace for the SDR Task API.
namespace sdr {

// ─── constants ──────────────────────────────────────────────────────────

/// @brief Magic bytes at the start of every IQ UDP packet ("IQP0").
inline constexpr uint32_t    IQ_PACKET_MAGIC       = 0x49515030u;
/// @brief Fixed size of the IqPacketHeader in bytes.
inline constexpr uint32_t    IQ_PACKET_HEADER_SIZE = 32u;
/// @brief Sentinel value for "no end time" (task runs indefinitely).
inline constexpr int64_t     TIME_INFINITE          = std::numeric_limits<int64_t>::max();
/// @brief AMQP message schema version emitted by this library build.
inline constexpr const char* SCHEMA_VERSION        = "2.0";
/// @brief Library version string.
inline constexpr const char* APP_VERSION           = "2.0.0";

/// @brief IQ packet flag: hardware overflow occurred in this packet.
inline constexpr uint8_t IQ_FLAG_OVERFLOW      = 0x01u;
/// @brief IQ packet flag: this is the first packet after a task start or retune.
inline constexpr uint8_t IQ_FLAG_FIRST_PACKET  = 0x02u;
/// @brief IQ packet flag: center frequency changed since the previous packet.
inline constexpr uint8_t IQ_FLAG_DWELL_CHANGE  = 0x04u;

// ─── task types ─────────────────────────────────────────────────────────

/**
 * @brief Identifies the DSP algorithm a task requires from the controller.
 *
 * The controller uses this to select hardware resources and stream parameters.
 * UNKNOWN is returned by taskTypeFromString() when the string is unrecognised.
 */
enum class TaskType : uint8_t {
    DF,          ///< Direction finding (coherent multi-channel).
    NARROWBAND,  ///< Narrowband demodulation (FM, AM, SSB, …).
    WIDEBAND,    ///< Wideband IQ capture / signal detection.
    SCAN,        ///< Stepped-frequency sweep (used by AcquisitionApp).
    SNAPSHOT,    ///< Single-frequency FFT power snapshot.
    TRIGGERED,   ///< Capture on threshold crossing.
    CALIBRATION, ///< Phase/amplitude calibration across boards.
    UNKNOWN      ///< Unrecognised task type string.
};

/// @brief Convert a string to TaskType; returns UNKNOWN on no match.
inline TaskType taskTypeFromString(std::string_view s) noexcept {
    if (s == "DF")          return TaskType::DF;
    if (s == "NARROWBAND")  return TaskType::NARROWBAND;
    if (s == "WIDEBAND")    return TaskType::WIDEBAND;
    if (s == "SCAN")        return TaskType::SCAN;
    if (s == "SNAPSHOT")    return TaskType::SNAPSHOT;
    if (s == "TRIGGERED")   return TaskType::TRIGGERED;
    if (s == "CALIBRATION") return TaskType::CALIBRATION;
    return TaskType::UNKNOWN;
}
/// @brief Convert a TaskType to its canonical string representation.
inline std::string taskTypeToString(TaskType t) {
    switch(t){
        case TaskType::DF:          return "DF";
        case TaskType::NARROWBAND:  return "NARROWBAND";
        case TaskType::WIDEBAND:    return "WIDEBAND";
        case TaskType::SCAN:        return "SCAN";
        case TaskType::SNAPSHOT:    return "SNAPSHOT";
        case TaskType::TRIGGERED:   return "TRIGGERED";
        case TaskType::CALIBRATION: return "CALIBRATION";
        default:                    return "UNKNOWN";
    }
}

// ─── schedule modes ─────────────────────────────────────────────────────

/**
 * @brief When the controller should start a task.
 */
enum class ScheduleMode : uint8_t {
    SCHEDULED,  ///< Start at the requested wall-clock time.
    IMMEDIATE,  ///< Start as soon as resources are available.
    CONTINUOUS  ///< Run indefinitely; no end time.
};
/// @brief Convert a string to ScheduleMode; defaults to SCHEDULED.
inline ScheduleMode scheduleModeFromString(std::string_view s) noexcept {
    if (s == "IMMEDIATE")  return ScheduleMode::IMMEDIATE;
    if (s == "CONTINUOUS") return ScheduleMode::CONTINUOUS;
    return ScheduleMode::SCHEDULED;
}
/// @brief Convert a ScheduleMode to its canonical string.
inline std::string scheduleModeToString(ScheduleMode m) {
    switch(m){
        case ScheduleMode::SCHEDULED:  return "SCHEDULED";
        case ScheduleMode::IMMEDIATE:  return "IMMEDIATE";
        case ScheduleMode::CONTINUOUS: return "CONTINUOUS";
    }
    return "SCHEDULED";
}

// ─── task states ────────────────────────────────────────────────────────

/**
 * @brief Lifecycle state of a task inside the controller.
 *
 * Terminal states are COMPLETED, FAILED, and CANCELLED.
 * Use isTerminalState() to test.
 */
enum class TaskState : uint8_t {
    EVALUATING,  ///< Request received; scheduler is assessing feasibility.
    SCHEDULED,   ///< Accepted and waiting for its start time.
    PENDING,     ///< Start time reached; hardware activation in progress.
    RUNNING,     ///< Hardware active; IQ stream flowing.
    COMPLETING,  ///< End time reached; draining in-flight packets.
    COMPLETED,   ///< Task finished normally.
    FAILED,      ///< Hardware error or timeout.
    CANCELLED    ///< Stopped by request or preempted by a higher-rank task.
};
/// @brief Convert a TaskState to its canonical string representation.
inline std::string taskStateToString(TaskState s) {
    switch(s){
        case TaskState::EVALUATING:  return "EVALUATING";
        case TaskState::SCHEDULED:   return "SCHEDULED";
        case TaskState::PENDING:     return "PENDING";
        case TaskState::RUNNING:     return "RUNNING";
        case TaskState::COMPLETING:  return "COMPLETING";
        case TaskState::COMPLETED:   return "COMPLETED";
        case TaskState::FAILED:      return "FAILED";
        case TaskState::CANCELLED:   return "CANCELLED";
    }
    return "UNKNOWN";
}
/// @brief Returns true if @p s is a terminal state (no further transitions).
inline bool isTerminalState(TaskState s) {
    return s==TaskState::COMPLETED || s==TaskState::FAILED || s==TaskState::CANCELLED;
}

/// @brief terminal_reason value set when a task is cancelled by preemption.
inline constexpr const char* PREEMPT_TERMINAL_REASON = "PREEMPTED_BY_HIGHER_RANK";

// ─── reject codes ────────────────────────────────────────────────────────

/**
 * @brief Machine-readable reason why a task request was rejected.
 */
enum class RejectCode : uint8_t {
    NONE,                    ///< Not a rejection (accepted).
    INVALID_REQUEST,         ///< Malformed JSON or missing required field.
    INVALID_TASK_TYPE,       ///< Unknown task_type string.
    INVALID_SCHEDULE,        ///< Conflicting or invalid schedule fields.
    FREQ_OUT_OF_RANGE,       ///< Requested center_freq_hz outside all device ranges.
    BW_EXCEEDED,             ///< Requested bandwidth exceeds device capability.
    SAMPLE_RATE_EXCEEDED,    ///< Requested sample rate exceeds device maximum.
    CHANNEL_COUNT_EXCEEDED,  ///< More RX/TX channels requested than available.
    SPECTRUM_CONFLICT,       ///< Another task already occupies the requested spectrum.
    TIME_CONFLICT,           ///< Requested time window overlaps an existing task.
    RETUNE_CONFLICT,         ///< Combined-window retune would exceed device bandwidth.
    NO_DEVICE_AVAILABLE,     ///< No device satisfies the request constraints.
    COHERENCY_UNAVAILABLE,   ///< Requested coherency_group has no free devices.
    TASK_LIMIT_REACHED,      ///< Global concurrent task limit hit.
    TASK_NOT_FOUND,          ///< task_id in STOP/CANCEL refers to an unknown task.
    TASK_NOT_STOPPABLE,      ///< Task is in a terminal state; cannot stop.
    TASK_ALREADY_TERMINAL,   ///< Task finished before the stop arrived.
    PORT_POOL_EXHAUSTED,     ///< All UDP ports in the pool are allocated.
    SCAN_ENTRY_INVALID,      ///< A scan plan entry has an invalid frequency or rate.
    DEVICE_OFFLINE,          ///< Target device is offline (SoapySDR open failed).
    INTERNAL_ERROR           ///< Unexpected internal scheduler error.
};
/// @brief Convert a RejectCode to its canonical string.
inline std::string rejectCodeToString(RejectCode c) {
    switch(c){
        case RejectCode::NONE:                   return "NONE";
        case RejectCode::INVALID_REQUEST:        return "INVALID_REQUEST";
        case RejectCode::INVALID_TASK_TYPE:      return "INVALID_TASK_TYPE";
        case RejectCode::INVALID_SCHEDULE:       return "INVALID_SCHEDULE";
        case RejectCode::FREQ_OUT_OF_RANGE:      return "FREQ_OUT_OF_RANGE";
        case RejectCode::BW_EXCEEDED:            return "BW_EXCEEDED";
        case RejectCode::SAMPLE_RATE_EXCEEDED:   return "SAMPLE_RATE_EXCEEDED";
        case RejectCode::CHANNEL_COUNT_EXCEEDED: return "CHANNEL_COUNT_EXCEEDED";
        case RejectCode::SPECTRUM_CONFLICT:      return "SPECTRUM_CONFLICT";
        case RejectCode::TIME_CONFLICT:          return "TIME_CONFLICT";
        case RejectCode::RETUNE_CONFLICT:        return "RETUNE_CONFLICT";
        case RejectCode::NO_DEVICE_AVAILABLE:    return "NO_DEVICE_AVAILABLE";
        case RejectCode::COHERENCY_UNAVAILABLE:  return "COHERENCY_UNAVAILABLE";
        case RejectCode::TASK_LIMIT_REACHED:     return "TASK_LIMIT_REACHED";
        case RejectCode::TASK_NOT_FOUND:         return "TASK_NOT_FOUND";
        case RejectCode::TASK_NOT_STOPPABLE:     return "TASK_NOT_STOPPABLE";
        case RejectCode::TASK_ALREADY_TERMINAL:  return "TASK_ALREADY_TERMINAL";
        case RejectCode::PORT_POOL_EXHAUSTED:    return "PORT_POOL_EXHAUSTED";
        case RejectCode::SCAN_ENTRY_INVALID:     return "SCAN_ENTRY_INVALID";
        case RejectCode::DEVICE_OFFLINE:         return "DEVICE_OFFLINE";
        case RejectCode::INTERNAL_ERROR:         return "INTERNAL_ERROR";
    }
    return "UNKNOWN";
}

// ─── task-type parameter blocks ─────────────────────────────────────────

/// @brief Parameters specific to direction-finding tasks.
struct DfParams {
    std::string algorithm       = "MUSIC"; ///< Algorithm name ("MUSIC", "ESPRIT", …).
    int         num_sources     = 1;       ///< Expected simultaneous sources.
    int         snapshot_count  = 1024;    ///< Samples per covariance snapshot.
    double      angular_res_deg = 1.0;     ///< Desired angular resolution (degrees).
};

/// @brief Parameters specific to narrowband demodulation tasks.
struct NarrowbandParams {
    std::string demod            = "FM";    ///< Demodulation mode ("FM","AM","SSB","CW",…).
    double      squelch_dbfs     = -80.0;   ///< Open squelch above this power level (dBFS).
    int         output_rate_sps  = 48000;   ///< Audio/baseband output sample rate.
};

/// @brief Parameters specific to wideband IQ capture tasks.
struct WidebandParams {
    bool   record_raw_iq         = true;   ///< Write raw IQ to storage.
    double detect_threshold_dbfs = -60.0;  ///< Signal detection threshold (dBFS).
    int    fft_size              = 2048;   ///< FFT size for wideband power display.
};

/// @brief One frequency step in a scan plan.
struct ScanEntry {
    int    step            = 0;    ///< Zero-based step index in the plan.
    double center_freq_hz  = 0.0;  ///< Centre frequency for this step (Hz).
    double bandwidth_hz    = 0.0;  ///< Requested bandwidth (Hz).
    double sample_rate_sps = 0.0;  ///< Requested sample rate (samples/s).
    int    dwell_ms        = 1000; ///< Dwell duration at this step (ms).
};

/// @brief Parameters specific to stepped-frequency scan tasks.
struct ScanParams {
    bool                   repeat  = true; ///< Repeat the plan continuously until stopped.
    std::vector<ScanEntry> entries;        ///< Ordered list of frequency steps.
};

/// @brief Parameters for a single-frequency FFT snapshot task.
struct SnapshotParams {
    double      center_freq_hz   = 0.0; ///< Centre frequency (Hz).
    double      bandwidth_hz     = 0.0; ///< Requested bandwidth (Hz).
    double      sample_rate_sps  = 0.0; ///< Requested sample rate (samples/s).
    int         fft_size         = 4096; ///< FFT size for power computation.
    int         n_averages       = 16;   ///< Number of FFT frames to average.
    std::string preferred_device;        ///< Device ID hint; empty = any device.
};

/// @brief Parameters for triggered IQ capture tasks.
struct TriggerParams {
    std::string trigger_type     = "POWER_THRESHOLD"; ///< Trigger condition type.
    double      threshold_dbfs   = -60.0; ///< Trigger level (dBFS).
    int         pre_trigger_ms   = 50;    ///< Samples to retain before the trigger (ms).
    int         post_trigger_ms  = 200;   ///< Capture duration after the trigger (ms).
    int         max_captures     = 0;     ///< Maximum captures (0 = unlimited).
};

/// @brief Parameters for multi-board coherent calibration tasks.
struct CalibrationParams {
    double      center_freq_hz      = 0.0; ///< Common LO frequency for all boards (Hz).
    double      bandwidth_hz        = 0.0; ///< Requested bandwidth (Hz).
    double      sample_rate_sps     = 0.0; ///< Requested sample rate (samples/s).
    int         duration_ms         = 5000; ///< Capture duration (ms).
    int         rx_count_per_device = 2;    ///< Channels per device in the group.
    std::string coherency_group;            ///< Coherency group name to calibrate.
    std::vector<std::string> devices;       ///< Explicit device list (empty = use group).
};

// ─── RF request ──────────────────────────────────────────────────────────

/**
 * @brief RF hardware requirements embedded in a TaskRequest.
 *
 * The scheduler uses these to find a device that satisfies every constraint.
 * Fields left at their zero/empty default are treated as "don't care".
 */
struct RfRequest {
    double              center_freq_hz    = 0.0; ///< Requested centre frequency (Hz).
    double              bandwidth_hz      = 0.0; ///< Requested RF bandwidth (Hz).
    double              sample_rate_sps   = 0.0; ///< Requested sample rate (samples/s).
    int                 rx_count          = 0;   ///< Number of RX channels required.
    int                 tx_count          = 0;   ///< Number of TX channels required.
    /// Preferred physical channel index. −1 = any.  If the channel is occupied at
    /// the same CF the scheduler subscribes to the existing stream; at a different
    /// CF it widens the channel window to cover both tasks (tryRetuneCombined).
    int                 preferred_channel = -1;
    std::vector<double> rx_gain_db;       ///< Per-channel RX gain (dB). Empty = default.
    std::vector<bool>   rx_agc;           ///< Per-channel AGC enable. Empty = disabled.
    std::vector<double> tx_atten_db;      ///< Per-channel TX attenuation (dB).
    std::string         preferred_device; ///< Device ID hint; empty = best fit.
    std::string         coherency_group;  ///< Required coherency group; empty = any.
};

/// @brief Destination for IQ UDP streams assigned by the controller.
struct StreamingDest {
    std::string      dest_ip;    ///< Destination IP address (must be routable from controller).
    std::vector<int> dest_ports; ///< One UDP port per RX channel.
};

// ─── decoded task request ───────────────────────────────────────────────

/**
 * @brief Fully decoded inbound task request from a DSP application.
 *
 * Produced by MessageCodec::decode() from the raw JSON AMQP body.
 * The controller reads this struct to decide accept/reject/schedule.
 */
struct TaskRequest {
    std::string   msg_type;         ///< "TASK_REQUEST_*", "TASK_STOP", "TASK_CANCEL", etc.
    std::string   schema_version;   ///< Sender's schema version string.
    std::string   request_id;       ///< UUID chosen by the sender; echoed in the response.
    std::string   correlation_id;   ///< Optional caller correlation token.
    int64_t       timestamp_ms    = 0; ///< Request creation time (UTC epoch ms).

    TaskType      task_type       = TaskType::UNKNOWN;         ///< DSP algorithm type.
    ScheduleMode  schedule_mode   = ScheduleMode::SCHEDULED;   ///< Scheduling policy.
    int           priority        = 0; ///< Priority within the same rank (higher = preferred).
    int           rank            = 0; ///< Preemption tier (higher rank preempts lower).

    int64_t       start_time_ms   = 0;            ///< Requested start (UTC epoch ms; 0 = immediate).
    int64_t       end_time_ms     = TIME_INFINITE; ///< Requested end (UTC epoch ms).
    int64_t       duration_ms     = 0;             ///< Duration hint (overrides end_time if > 0).

    std::string   task_id;   ///< For STOP/CANCEL: the task to act on.
    std::string   reason;    ///< Human-readable reason for STOP/CANCEL.

    RfRequest     rf;        ///< RF hardware requirements.
    StreamingDest streaming; ///< Where to send the IQ stream.

    std::optional<DfParams>          df_params;       ///< DF-specific parameters.
    std::optional<NarrowbandParams>  nb_params;       ///< Narrowband-specific parameters.
    std::optional<WidebandParams>    wb_params;       ///< Wideband-specific parameters.
    std::optional<ScanParams>        scan_params;     ///< Scan-specific parameters.
    std::optional<SnapshotParams>    snapshot_params; ///< Snapshot-specific parameters.
    std::optional<TriggerParams>     trigger_params;  ///< Trigger-specific parameters.
    std::optional<CalibrationParams> cal_params;      ///< Calibration-specific parameters.
};

// ─── assigned stream (in response) ──────────────────────────────────────

/**
 * @brief Describes one IQ UDP stream assigned by the controller.
 *
 * One AssignedStream per RX channel is included in the TaskResponse.
 * The DSP application listens on @p udp_ip : @p udp_port for CF32 packets.
 */
struct AssignedStream {
    std::string stream_id;              ///< Unique stream identifier.
    std::string device_id;             ///< Device that owns this stream.
    std::string channel_type;          ///< "RX" or "TX".
    int         channel_index  = 0;   ///< Physical channel index on the device.
    std::string udp_ip;               ///< Source IP for IQ packets.
    int         udp_port       = 0;   ///< Destination UDP port for IQ packets.
    double      center_freq_hz = 0.0; ///< Actual LO centre frequency (Hz).
    double      slice_offset_hz= 0.0; ///< DDC offset from LO to this slice centre (Hz).
    double      slice_bw_hz    = 0.0; ///< Usable bandwidth of this slice (Hz).
    double      sample_rate_sps= 0.0; ///< Actual sample rate (samples/s).
    std::string format         = "CF32"; ///< IQ sample format ("CF32" = interleaved float32).
};

// ─── task response ───────────────────────────────────────────────────────

/**
 * @brief Response sent by the controller for every task request.
 *
 * Produced by MessageCodec::encodeTaskResponse() and sent to the requesting
 * application's dynamic reply-to address.
 */
struct TaskResponse {
    std::string  request_id;               ///< Echoed from the originating TaskRequest.
    std::string  correlation_id;           ///< Echoed from the originating TaskRequest.
    bool         accepted        = false;  ///< True if the task was accepted.
    std::string  task_id;                  ///< Assigned task UUID (set when accepted).
    std::string  schedule_mode;            ///< Effective schedule mode string.
    int64_t      actual_start_ms = 0;      ///< Actual start time (UTC epoch ms).
    int64_t      actual_stop_ms  = 0;      ///< Planned stop time (UTC epoch ms).
    RejectCode   reject_code     = RejectCode::NONE; ///< Reason for rejection (if !accepted).
    std::string  reject_reason;            ///< Human-readable rejection message.
    std::vector<AssignedStream> streams;   ///< One entry per assigned RX channel.
};

// ─── stream metrics ──────────────────────────────────────────────────────

/// @brief Live per-stream statistics published in TASK_STATUS messages.
struct StreamMetrics {
    std::string stream_id;              ///< Identifies the stream.
    std::string channel_type;           ///< "RX" or "TX".
    int         channel_index   = 0;   ///< Physical channel index.
    int         udp_port        = 0;   ///< UDP destination port.
    uint64_t    samples_total   = 0;   ///< Total samples delivered since task start.
    uint64_t    packets_sent    = 0;   ///< Total UDP packets sent.
    uint32_t    overflows       = 0;   ///< SoapySDR overflow count.
    float       rssi_dbfs       = 0.0f; ///< Instantaneous RSSI (dBFS).
    double      throughput_mbps = 0.0;  ///< Current UDP throughput (Mbps).
};

// ─── IQ packet header — binary, LE, 32 bytes ─────────────────────────────

/**
 * @brief Binary header prepended to every IQ UDP packet.
 *
 * Layout: 32 bytes, little-endian, packed (no padding).
 * The payload immediately follows: @p num_samples interleaved CF32 samples.
 *
 * Receivers identify packets by the @p magic field (IQ_PACKET_MAGIC = "IQP0").
 * On scan tasks, @p center_freq_hz updates on each dwell change and
 * IQ_FLAG_DWELL_CHANGE is set in @p flags.
 */
#pragma pack(push, 1)
struct IqPacketHeader {
    uint32_t magic;           ///< IQ_PACKET_MAGIC — identifies this as an IQ packet.
    uint32_t sequence;        ///< Monotonically increasing packet counter (wraps at 2^32).
    uint64_t timestamp_ns;    ///< Nanoseconds since task start.
    uint64_t center_freq_hz;  ///< Current LO centre frequency (updated on scan dwell change).
    uint32_t sample_rate;     ///< Sample rate of the payload (samples/s, integer).
    uint16_t num_samples;     ///< Number of CF32 samples in the payload.
    uint8_t  channel_index;   ///< RX channel index (0-based).
    uint8_t  flags;           ///< Bitmask of IQ_FLAG_* constants.
};
static_assert(sizeof(IqPacketHeader) == IQ_PACKET_HEADER_SIZE);
#pragma pack(pop)

// ─── internal task record ────────────────────────────────────────────────

/**
 * @brief Controller-internal record for a live or recently-completed task.
 *
 * Created when a task is accepted, updated on every state transition,
 * and passed to the TaskStateChangedCb callback.  DSP applications never
 * see this struct directly — they receive TaskResponse and TASK_STATUS JSON.
 */
struct TaskRecord {
    std::string   task_id;
    std::string   request_id;
    std::string   correlation_id;
    TaskType      task_type     = TaskType::UNKNOWN;
    ScheduleMode  schedule_mode = ScheduleMode::SCHEDULED;
    TaskState     state         = TaskState::EVALUATING;
    int           priority      = 0;
    int           rank          = 0;
    int64_t       start_time_ms = 0;
    int64_t       stop_time_ms  = TIME_INFINITE;

    /// @brief Hardware allocation for one device in this task.
    struct DeviceAllocation {
        std::string      device_id;
        std::vector<int> rx_channels;          ///< Physical RX channel indices.
        std::vector<int> tx_channels;          ///< Physical TX channel indices.
        std::vector<int> udp_ports;            ///< Allocated UDP ports.
        double           center_freq_hz       = 0.0; ///< Device LO frequency.
        double           sample_rate_sps      = 0.0; ///< Device (wideband) sample rate.
        double           output_sample_rate_sps = 0.0; ///< After DDC; 0 = same as device rate.
        double           slice_lo_hz          = 0.0; ///< Lower edge of the allocated slice.
        double           slice_hi_hz          = 0.0; ///< Upper edge of the allocated slice.
    };
    std::vector<DeviceAllocation> allocations; ///< One entry per participating device.

    StreamingDest streaming;

    std::optional<ScanParams>        scan_params;
    std::optional<TriggerParams>     trigger_params;
    std::optional<CalibrationParams> cal_params;

    std::chrono::steady_clock::time_point accepted_at;   ///< When the task was accepted.
    std::vector<StreamMetrics>            stream_metrics; ///< Live metrics, updated by IQStreamer.
    std::string                           terminal_reason; ///< Set on CANCELLED/FAILED.
};

/// @brief Returns true if the task was cancelled due to preemption.
inline bool isPreempted(const TaskRecord& r) {
    return r.state == TaskState::CANCELLED &&
           r.terminal_reason.find(PREEMPT_TERMINAL_REASON) != std::string::npos;
}

// ─── callbacks ───────────────────────────────────────────────────────────

/// @brief Called by the controller whenever a task's state changes.
using TaskStateChangedCb = std::function<void(const TaskRecord&)>;
/// @brief Called by the controller on unrecoverable task errors.
using TaskErrorCb        = std::function<void(const std::string& task_id,
                                               const std::string& error)>;

// ─── snapshot result (from FftEngine) ────────────────────────────────────

/**
 * @brief Result of a SNAPSHOT task — FFT power bins for one frequency.
 *
 * Produced by FftEngine and serialised by MessageCodec::encodeSnapshotResult().
 */
struct SnapshotResult {
    std::string         device_id;
    double              center_freq_hz     = 0.0; ///< Actual LO centre frequency (Hz).
    double              bandwidth_hz       = 0.0; ///< Captured bandwidth (Hz).
    double              sample_rate_sps    = 0.0; ///< Sample rate used (samples/s).
    int                 fft_size           = 0;   ///< FFT size.
    int                 n_averages         = 0;   ///< Number of frames averaged.
    double              freq_resolution_hz = 0.0; ///< Bin width (Hz) = sample_rate / fft_size.
    double              freq_axis_start_hz = 0.0; ///< Frequency of bin[0] (Hz).
    double              freq_axis_step_hz  = 0.0; ///< Frequency step per bin (Hz).
    std::vector<double> power_bins;               ///< Power in dBFS per FFT bin.
    bool                success            = false; ///< False if acquisition failed.
    std::string         error_msg;                  ///< Non-empty when success == false.
};

} // namespace sdr
