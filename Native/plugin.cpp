// ReSharper disable CppParameterMayBeConstPtrOrRef
// ReSharper disable CppDFAConstantFunctionResult
#include <atomic>
#include "AudioPluginUtil.h"
#include "dwm.h"
#include "plugin_config.h"

extern "C" {
int UNITY_AUDIODSP_EXPORT_API GetSampleRate() { return DWM_SAMPLE_RATE; }
int UNITY_AUDIODSP_EXPORT_API GetBufferSize() { return DWM_BUFFER_SIZE; }
int UNITY_AUDIODSP_EXPORT_API GetMaxSourceCount() { return DWM_MAX_SOURCE_COUNT; }
}

struct dwm_source_data_t {
    float p_x, p_y, p_z;
    float buffer[DWM_BUFFER_SIZE];
};

static dwm_source_data_t dwm_source_data[DWM_MAX_SOURCE_COUNT];

namespace DWM_Test_Source_Spatializer {
    enum param_t { param_gain, param_index, param_px, param_py, param_pz, param_num };

    struct data_t {
        float parameters[param_num];
    };

    int InternalRegisterEffectDefinition(UnityAudioEffectDefinition &definition) {
        definition.paramdefs = new UnityAudioParameterDefinition[param_num];
        AudioPluginUtil::RegisterParameter(definition, "Gain", "dB", //
                                           -100.0f, 100.0f, 0.0f, //
                                           1.0f, 1.0f, //
                                           param_gain, "Overall gain applied");
        AudioPluginUtil::RegisterParameter(definition, "Index", "n", //
                                           0.0f, DWM_BUFFER_SIZE - 1, 0.0f, //
                                           1.0f, 1.0f, //
                                           param_index, "DWM source index");
        AudioPluginUtil::RegisterParameter(definition, "Position X", "m", //
                                           0.0f, 1.0f, 0.0f, //
                                           1.0f, 1.0f, //
                                           param_px, "Position X in the DWM");
        AudioPluginUtil::RegisterParameter(definition, "Position Y", "m", //
                                           0.0f, 1.0f, 0.0f, //
                                           1.0f, 1.0f, //
                                           param_py, "Position Y in the DWM");
        AudioPluginUtil::RegisterParameter(definition, "Position Z", "m", //
                                           0.0f, 1.0f, 0.0f, //
                                           1.0f, 1.0f, //
                                           param_pz, "Position Z in the DWM");
        return param_num;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState *state) {
        auto *data = new data_t();
        AudioPluginUtil::InitParametersFromDefinitions(InternalRegisterEffectDefinition, data->parameters);
        state->effectdata = data;
        memset(dwm_source_data, 0, sizeof(dwm_source_data_t) * DWM_MAX_SOURCE_COUNT);
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ReleaseCallback(UnityAudioEffectState *state) {
        const auto *data = state->GetEffectData<data_t>();
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

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(UnityAudioEffectState *state, float *in_buffer,
                                                                  float *out_buffer, const unsigned int num_samples,
                                                                  const int in_channels, const int out_channels) {
        memset(out_buffer, 0, num_samples * out_channels * sizeof(float)); // Silence standard output

        const auto *data = state->GetEffectData<data_t>();

        const int index =
                std::clamp(0, DWM_MAX_SOURCE_COUNT - 1, static_cast<int>(std::round(data->parameters[param_index])));

        dwm_source_data_t *src_data = &dwm_source_data[index];
        src_data->p_x = data->parameters[param_px];
        src_data->p_y = data->parameters[param_py];
        src_data->p_z = data->parameters[param_pz];
        for (unsigned int n = 0; n < DWM_BUFFER_SIZE; n++)
            src_data->buffer[n] = in_buffer[n * in_channels];

        return UNITY_AUDIODSP_OK;
    }

} // namespace DWM_Test_Source_Spatializer


namespace DWM_Test_Mesh_Simulation {

    typedef dwm::filters::admittance_lowpass_parameters boundary_parameters;
    typedef dwm::filters::admittance_lowpass filter;
    typedef dwm::mesh_3d<filter, boundary_parameters, //
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
        return param_num;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState *state) {
        auto *data = new data_t();
        data->mesh = new mesh_admittance_lowpass(DWM_SAMPLE_RATE, 1.0f, 1.0f, 1.0f);
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
        const auto p = boundary_parameters(0.5f, 0.5f); // TODO: get from effect parameters

        const float gain = powf(10.0f, data->parameters[param_gain] * 0.05f);
        for (unsigned int n = 0; n < num_samples; n++) {
            for (auto &[p_x, p_y, p_z, buffer]: dwm_source_data) {
                data->mesh->write_value(p_x, p_y, p_z, buffer[n] * gain);
                buffer[n] = 0;
            }
            data->mesh->update(p, p, p, p, p, p);
            out_buffer[n * out_channels + 0] = data->mesh->read_value(0.4f, 0.5f, 0.5f);
            out_buffer[n * out_channels + 1] = data->mesh->read_value(0.6f, 0.5f, 0.5f);
            for (int i = 2; i < out_channels; i++) {
                out_buffer[n * out_channels + i] = 0.0f;
            }
        }

        return UNITY_AUDIODSP_OK;
    }

} // namespace DWM_Test_Mesh_Simulation
