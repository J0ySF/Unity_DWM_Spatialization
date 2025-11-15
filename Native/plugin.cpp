// ReSharper disable CppParameterMayBeConstPtrOrRef
// ReSharper disable CppDFAConstantFunctionResult
#include <atomic>
#include "AudioPluginUtil.h"
#include "binauraliser.h"
#include "dwm.h"
#include "plugin_config.h"

struct dwm_source_data_t {
    float p_x = 0.0f, p_y = 0.0f, p_z = 0.0f;
    bool active = false;
    float buffer[DWM_BUFFER_SIZE] = {};
};

static dwm_source_data_t dwm_source_data[DWM_MAX_SOURCE_COUNT] = {};

extern "C" {
int UNITY_AUDIODSP_EXPORT_API GetSampleRate() { return DWM_SAMPLE_RATE; }
int UNITY_AUDIODSP_EXPORT_API GetBufferSize() { return DWM_BUFFER_SIZE; }
int UNITY_AUDIODSP_EXPORT_API GetMaxSourceCount() { return DWM_MAX_SOURCE_COUNT; }
float UNITY_AUDIODSP_EXPORT_API GetMeshWidth() { return DWM_MESH_WIDTH; }
float UNITY_AUDIODSP_EXPORT_API GetMeshHeight() { return DWM_MESH_HEIGHT; }
float UNITY_AUDIODSP_EXPORT_API GetMeshDepth() { return DWM_MESH_DEPTH; }
float UNITY_AUDIODSP_EXPORT_API GetEarsDistance() { return DWM_EARS_DISTANCE; }
void UNITY_AUDIODSP_EXPORT_API WriteSource(const int index, const float p_x, const float p_y, const float p_z,
                                           float *buffer, const int num_channels) {
    dwm_source_data_t *src_data = &dwm_source_data[std::clamp(index, 0, DWM_MAX_SOURCE_COUNT - 1)];
    src_data->p_x = p_x;
    src_data->p_y = p_y;
    src_data->p_z = p_z;
    src_data->active = true;
    for (unsigned int n = 0; n < DWM_BUFFER_SIZE; n++)
        src_data->buffer[n] = buffer[n * num_channels];
}
}

namespace DWM_Mesh_Simulation {

    typedef dwm::filters::admittance_lowpass_parameters boundary_parameters;
    typedef dwm::filters::admittance_lowpass filter;
    typedef dwm::mesh_3d<DWM_MESH_WIDTH, DWM_MESH_HEIGHT, DWM_MESH_DEPTH, DWM_SAMPLE_RATE, //
                         filter, boundary_parameters, //
                         filter, boundary_parameters, //
                         filter, boundary_parameters, filter, boundary_parameters, filter, //
                         boundary_parameters, filter, boundary_parameters> //
            mesh_admittance_lowpass;

    enum param_t {
        param_gain,
        param_admittance_xp,
        param_cutoff_xp, //
        param_admittance_xn,
        param_cutoff_xn, //
        param_admittance_yp,
        param_cutoff_yp, //
        param_admittance_yn,
        param_cutoff_yn, //
        param_admittance_zp,
        param_cutoff_zp, //
        param_admittance_zn,
        param_cutoff_zn, //
        param_num
    };

    struct data_t {
        float parameters[param_num];
        mesh_admittance_lowpass *mesh;
        void *binauraliser;
    };

    int InternalRegisterEffectDefinition(UnityAudioEffectDefinition &definition) {
        definition.paramdefs = new UnityAudioParameterDefinition[param_num];
        AudioPluginUtil::RegisterParameter(definition, "Gain", "dB", //
                                           -100.0f, 100.0f, 0.0f, //
                                           1.0f, 1.0f, //
                                           param_gain, "Overall gain applied");

        AudioPluginUtil::RegisterParameter(definition, "Admittance X+", "%", //
                                           0.0f, 1.0f, 0.0f, //
                                           1.0f, 1.0f, //
                                           param_admittance_xp, "Admittance X+");
        AudioPluginUtil::RegisterParameter(definition, "Cutoff X+", "%", //
                                           0.0f, 1.0f, 0.0f, //
                                           1.0f, 1.0f, //
                                           param_cutoff_xp, "Cutoff X+");
        AudioPluginUtil::RegisterParameter(definition, "Admittance X-", "%", //
                                           0.0f, 1.0f, 0.0f, //
                                           1.0f, 1.0f, //
                                           param_admittance_xn, "Admittance X-");
        AudioPluginUtil::RegisterParameter(definition, "Cutoff X-", "%", //
                                           0.0f, 1.0f, 0.0f, //
                                           1.0f, 1.0f, //
                                           param_cutoff_xn, "Cutoff X-");

        AudioPluginUtil::RegisterParameter(definition, "Admittance Y+", "%", //
                                           0.0f, 1.0f, 0.0f, //
                                           1.0f, 1.0f, //
                                           param_admittance_yp, "Admittance Y+");
        AudioPluginUtil::RegisterParameter(definition, "Cutoff Y+", "%", //
                                           0.0f, 1.0f, 0.0f, //
                                           1.0f, 1.0f, //
                                           param_cutoff_yp, "Cutoff Y+");
        AudioPluginUtil::RegisterParameter(definition, "Admittance Y-", "%", //
                                           0.0f, 1.0f, 0.0f, //
                                           1.0f, 1.0f, //
                                           param_admittance_yn, "Admittance Y-");
        AudioPluginUtil::RegisterParameter(definition, "Cutoff Y-", "%", //
                                           0.0f, 1.0f, 0.0f, //
                                           1.0f, 1.0f, //
                                           param_cutoff_yn, "Cutoff Y-");

        AudioPluginUtil::RegisterParameter(definition, "Admittance Z+", "%", //
                                           0.0f, 1.0f, 0.0f, //
                                           1.0f, 1.0f, //
                                           param_admittance_zp, "Admittance Z+");
        AudioPluginUtil::RegisterParameter(definition, "Cutoff Z+", "%", //
                                           0.0f, 1.0f, 0.0f, //
                                           1.0f, 1.0f, //
                                           param_cutoff_zp, "Cutoff Z+");
        AudioPluginUtil::RegisterParameter(definition, "Admittance Z-", "%", //
                                           0.0f, 1.0f, 0.0f, //
                                           1.0f, 1.0f, //
                                           param_admittance_zn, "Admittance Z-");
        AudioPluginUtil::RegisterParameter(definition, "Cutoff Z-", "%", //
                                           0.0f, 1.0f, 0.0f, //
                                           1.0f, 1.0f, //
                                           param_cutoff_zn, "Cutoff Z-");

        definition.flags |= UnityAudioEffectDefinitionFlags_NeedsSpatializerData;
        return param_num;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState *state) {
        auto *data = new data_t();
        data->mesh = new mesh_admittance_lowpass();
        binauraliser_create(&data->binauraliser);
        binauraliser_init(data->binauraliser, DWM_SAMPLE_RATE);
        binauraliser_setNumSources(data->binauraliser, DWM_MAX_SOURCE_COUNT);
        binauraliser_initCodec(data->binauraliser);
        AudioPluginUtil::InitParametersFromDefinitions(InternalRegisterEffectDefinition, data->parameters);
        state->effectdata = data;
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ReleaseCallback(UnityAudioEffectState *state) {
        auto *data = state->GetEffectData<data_t>();
        delete data->mesh;
        binauraliser_destroy(&data->binauraliser);
        delete data;
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK SetFloatParameterCallback(UnityAudioEffectState *state,
                                                                            const int index, const float value) {
        auto *data = state->GetEffectData<data_t>();
        if (index >= param_num)
            return UNITY_AUDIODSP_ERR_UNSUPPORTED;
        data->parameters[index] = value;
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK GetFloatParameterCallback(UnityAudioEffectState *state,
                                                                            const int index, float *value,
                                                                            char *value_str) {
        const auto *data = state->GetEffectData<data_t>();
        if (value != nullptr)
            *value = data->parameters[index];
        if (value_str != nullptr)
            value_str[0] = 0;
        return UNITY_AUDIODSP_OK;
    }

    int UNITY_AUDIODSP_CALLBACK GetFloatBufferCallback(UnityAudioEffectState *, const char *, float *buffer,
                                                       const int num_samples) {
        memset(buffer, 0, sizeof(float) * num_samples);
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(UnityAudioEffectState *state, float *,
                                                                  float *out_buffer, const unsigned int num_samples,
                                                                  const int, const int out_channels) {
        const auto *data = state->GetEffectData<data_t>();
        const float *m = state->spatializerdata->listenermatrix;

        const float listen_x = -(m[0] * m[12] + m[1] * m[13] + m[2] * m[14]);
        const float listen_y = -(m[4] * m[12] + m[5] * m[13] + m[6] * m[14]);
        const float listen_z = -(m[8] * m[12] + m[9] * m[13] + m[10] * m[14]);

        const float listen_right_x = m[0] * DWM_EARS_DISTANCE;
        const float listen_right_y = m[4] * DWM_EARS_DISTANCE;
        const float listen_right_z = m[8] * DWM_EARS_DISTANCE;

        const float listen_l_x = listen_x - listen_right_x;
        const float listen_l_y = listen_y - listen_right_y;
        const float listen_l_z = listen_z - listen_right_z;

        const float listen_r_x = listen_x + listen_right_x;
        const float listen_r_y = listen_y + listen_right_y;
        const float listen_r_z = listen_z + listen_right_z;

        const auto p_xp = boundary_parameters(data->parameters[param_admittance_xp], data->parameters[param_cutoff_xp]);
        const auto p_xn = boundary_parameters(data->parameters[param_admittance_xn], data->parameters[param_cutoff_xn]);
        const auto p_yp = boundary_parameters(data->parameters[param_admittance_yp], data->parameters[param_cutoff_yp]);
        const auto p_yn = boundary_parameters(data->parameters[param_admittance_yn], data->parameters[param_cutoff_yn]);
        const auto p_zp = boundary_parameters(data->parameters[param_admittance_zp], data->parameters[param_cutoff_zp]);
        const auto p_zn = boundary_parameters(data->parameters[param_admittance_zn], data->parameters[param_cutoff_zn]);

        const float gain = powf(10.0f, data->parameters[param_gain] * 0.05f);
        float intermediate[DWM_BUFFER_SIZE];
        for (unsigned int n = 0; n < num_samples; n++) {
            for (auto &[p_x, p_y, p_z, active, buffer]: dwm_source_data) {
                data->mesh->write_value(p_x, p_y, p_z, buffer[n] * gain);
                buffer[n] = 0;
            }
            data->mesh->update(p_xp, p_xn, p_yp, p_yn, p_zp, p_zn);
            intermediate[n] = data->mesh->read_value(listen_x, listen_y, listen_z);
        }
        int in_count = 0;
        const float *in[DWM_MAX_SOURCE_COUNT];
        for (int i = 0; i < DWM_MAX_SOURCE_COUNT; i++) {
            in[i] = intermediate;
            if (!dwm_source_data[i].active)
                continue;
            const float d_x = dwm_source_data[i].p_x - listen_x;
            const float d_y = dwm_source_data[i].p_y - listen_y;
            const float d_z = dwm_source_data[i].p_z - listen_z;
            const float delta_r_x = m[0] * d_x + m[4] * d_y + m[8] * d_z;
            const float delta_r_y = m[1] * d_x + m[5] * d_y + m[9] * d_z;
            const float delta_r_z = m[2] * d_x + m[6] * d_y + m[10] * d_z;
            binauraliser_setSourceAzi_deg(data->binauraliser, in_count,
                                          -std::atan2(delta_r_x, delta_r_z) / AudioPluginUtil::kPI * 180.0f);
            binauraliser_setSourceElev_deg(
                    data->binauraliser, in_count,
                    std::atan2(delta_r_y, std::sqrt(delta_r_x * delta_r_x + delta_r_z * delta_r_z)) /
                            AudioPluginUtil::kPI * 180.0f);
            in_count++;
        }
        float out_l[DWM_BUFFER_SIZE], out_r[DWM_BUFFER_SIZE];
        float *out[2] = {out_l, out_r};
        binauraliser_process(data->binauraliser, in, out, in_count, 2, DWM_BUFFER_SIZE);
        for (unsigned int n = 0; n < num_samples; n++) {
            out_buffer[n * out_channels + 0] = out_l[n];
            out_buffer[n * out_channels + 1] = out_r[n];
            for (int i = 2; i < out_channels; i++) {
                out_buffer[n * out_channels + i] = 0.0f;
            }
        }

        for (int i = 0; i < DWM_MAX_SOURCE_COUNT; i++) {
            dwm_source_data[i].active = false;
        }


        return UNITY_AUDIODSP_OK;
    }

} // namespace DWM_Mesh_Simulation
