#pragma once
/**
 * @file MessageCodec.hpp
 * @brief AMQP message encode/decode for the SDR task protocol.
 *
 * All JSON bodies exchanged between SdrResourceManager and DSP applications
 * (AcquisitionApp, AnalysisApp, DfApp) are serialised and deserialised here.
 * Consumers depend on this file via the `sdr/` include path.
 */
#include "Types.hpp"
#include <string>
#include <optional>
#include <vector>

namespace sdr {

/**
 * @brief Static encode/decode methods for the SDR AMQP message protocol.
 *
 * All methods are static — MessageCodec has no instance state.
 *
 * **Decode flow** (controller inbound):
 * 1. Call peekMsgType() to identify the message without full parse.
 * 2. Call decode() to get a fully populated TaskRequest.
 *
 * **Encode flow** (controller outbound):
 * - encodeTaskResponse()       → reply to TASK_REQUEST_*
 * - encodeTaskStatus()         → periodic TASK_STATUS events
 * - encodeSnapshotResult()     → reply to TASK_REQUEST_SNAPSHOT
 * - encodeDeviceHealth()       → per-device section of HEALTH_QUERY_RESPONSE
 * - encodeControllerHealth()   → controller-level section
 * - encodeHealthQueryResponse()→ combined response (devices + controller)
 * - encodeTempResponse()       → reply to DEVICE_TEMP_QUERY
 */
class MessageCodec {
public:
    /**
     * @brief Extract the `msg_type` field without parsing the full message.
     * @param json Raw JSON AMQP body.
     * @return The msg_type string, or empty if the field is absent.
     */
    static std::string             peekMsgType(const std::string& json);

    /**
     * @brief Decode a full task request from a JSON AMQP body.
     * @param json Raw JSON AMQP body.
     * @return Populated TaskRequest, or std::nullopt if parsing fails.
     */
    static std::optional<TaskRequest> decode(const std::string& json);

    /**
     * @brief Encode a TaskResponse to JSON.
     * @param r The response to serialise.
     * @return JSON string suitable for sending as an AMQP message body.
     */
    static std::string encodeTaskResponse(const TaskResponse& r);

    /**
     * @brief Encode a TASK_STATUS event for a live task.
     * @param rec  Current TaskRecord state.
     * @param m    Stream metrics snapshot.
     * @return JSON string for the `sdr.status` topic.
     */
    static std::string encodeTaskStatus(const TaskRecord& rec,
                                        const std::vector<StreamMetrics>& m);

    /**
     * @brief Encode a SNAPSHOT_RESULT reply.
     * @param req_id  request_id from the originating snapshot request.
     * @param r       Computed snapshot result.
     * @return JSON string.
     */
    static std::string encodeSnapshotResult(const std::string& req_id,
                                            const SnapshotResult& r);

    /// @brief Per-device summary for health responses.
    struct DevHealthEntry {
        std::string device_id,     ///< Device identifier from devices.xml.
                    driver,        ///< SoapySDR driver name ("remote", "plutosdr", …).
                    uri,           ///< SoapySDR URI string.
                    coherency_group; ///< Coherency group name (empty = none).
        bool   online       = false; ///< True if device is currently open.
        int    active_tasks = 0;     ///< Number of tasks currently using this device.
        double cf_hz        = 0;     ///< Current LO centre frequency (Hz).
        double rate_sps     = 0;     ///< Current sample rate (samples/s).
        double alloc_bw_hz  = 0;     ///< Allocated bandwidth (Hz).
        double free_bw_hz   = 0;     ///< Remaining free bandwidth (Hz).
        double temp_c       = 0;     ///< Last reported temperature (°C).
    };

    /**
     * @brief Encode the per-device section of a HEALTH_QUERY_RESPONSE.
     * @param req_id  request_id from the originating health query.
     * @param e       Vector of per-device entries.
     * @return JSON string.
     */
    static std::string encodeDeviceHealth(const std::string& req_id,
                                          const std::vector<DevHealthEntry>& e);

    /**
     * @brief Encode the controller-level section of a health response.
     * @param req_id      request_id from the originating health query.
     * @param tot_dev     Total device count in config.
     * @param on_dev      Devices currently online.
     * @param sched       Tasks in SCHEDULED state.
     * @param pend        Tasks in PENDING state.
     * @param run         Tasks in RUNNING state.
     * @param tot_tasks   Total task count (all states).
     * @param ports_used  Allocated UDP ports.
     * @param ports_free  Available UDP ports.
     * @param uptime_sec  Controller uptime (seconds).
     * @return JSON string.
     */
    static std::string encodeControllerHealth(const std::string& req_id,
        int tot_dev, int on_dev, int sched, int pend, int run,
        int tot_tasks, int ports_used, int ports_free, int64_t uptime_sec);

    /**
     * @brief Encode a complete HEALTH_QUERY_RESPONSE (devices + controller).
     *
     * Combines encodeDeviceHealth() and encodeControllerHealth() into a
     * single message body.
     */
    static std::string encodeHealthQueryResponse(const std::string& req_id,
        const std::vector<DevHealthEntry>& devs,
        int tot_dev,int on_dev,int sched,int pend,int run,
        int tot_tasks,int ports_used,int ports_free,int64_t uptime_sec);

    /// @brief Temperature reading for one named sensor.
    struct TempSensor  {
        std::string name;    ///< Sensor name as reported by SoapySDR.
        double value_c;      ///< Temperature in degrees Celsius.
        bool valid;          ///< False if the read failed (value_c is NaN).
    };

    /// @brief All temperature sensors for one device.
    struct TempEntry {
        std::string            device_id; ///< Device identifier.
        bool                   online = false; ///< False if device is offline.
        std::vector<TempSensor> sensors;  ///< One entry per sensor.
    };

    /**
     * @brief Encode a DEVICE_TEMP_RESPONSE.
     * @param req_id  request_id from the originating temperature query.
     * @param devs    Per-device temperature data.
     * @return JSON string.
     */
    static std::string encodeTempResponse(const std::string& req_id,
                                          const std::vector<TempEntry>& devs);
};

} // namespace sdr
