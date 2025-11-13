// ReSharper disable CppParameterMayBeConstPtrOrRef
// ReSharper disable CppDFAConstantFunctionResult
#include <atomic>
#include "AudioPluginUtil.h"
#include "dwm.h"
#include "plugin_config.h"

struct dwm_source_data_t {
    float p_x = 0.0f, p_y = 0.0f, p_z = 0.0f;
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

    enum param_t { param_gain, param_num };

    struct data_t {
        float parameters[param_num];
        mesh_admittance_lowpass *mesh;
    };

    int InternalRegisterEffectDefinition(UnityAudioEffectDefinition &definition) {
        definition.paramdefs = new UnityAudioParameterDefinition[param_num];
        AudioPluginUtil::RegisterParameter(definition, "Gain", "dB", //
                                           -100.0f, 100.0f, 0.0f, //
                                           1.0f, 1.0f, //
                                           param_gain, "Overall gain applied");
        definition.flags |= UnityAudioEffectDefinitionFlags_NeedsSpatializerData;
        return param_num;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState *state) {
        auto *data = new data_t();
        data->mesh = new mesh_admittance_lowpass();
        AudioPluginUtil::InitParametersFromDefinitions(InternalRegisterEffectDefinition, data->parameters);
        state->effectdata = data;
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ReleaseCallback(UnityAudioEffectState *state) {
        const auto *data = state->GetEffectData<data_t>();
        delete data->mesh;
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

        const auto p = boundary_parameters(0.9f, 0.1f); // TODO: get from effect parameters

        const float gain = powf(10.0f, data->parameters[param_gain] * 0.05f);
        for (unsigned int n = 0; n < num_samples; n++) {
            for (auto &[p_x, p_y, p_z, buffer]: dwm_source_data) {
                data->mesh->write_value(p_x, p_y, p_z, buffer[n] * gain);
                buffer[n] = 0;
            }
            data->mesh->update(p, p, p, p, p, p);
            out_buffer[n * out_channels + 0] = data->mesh->read_value(listen_l_x, listen_l_y, listen_l_z);
            out_buffer[n * out_channels + 1] = data->mesh->read_value(listen_r_x, listen_r_y, listen_r_z);
            for (int i = 2; i < out_channels; i++) {
                out_buffer[n * out_channels + i] = 0.0f;
            }
        }

        return UNITY_AUDIODSP_OK;
    }

} // namespace DWM_Mesh_Simulation
