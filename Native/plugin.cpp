// ReSharper disable CppParameterMayBeConstPtrOrRef
// ReSharper disable CppDFAConstantFunctionResult
#include "AudioPluginUtil.h"
#include "dwm.h"
#include "plugin_config.h"

extern "C" {
int UNITY_AUDIODSP_EXPORT_API GetSampleRate() { return DWM_SAMPLE_RATE; }
int UNITY_AUDIODSP_EXPORT_API GetBufferSize() { return DWM_BUFFER_SIZE; }
}

namespace DWM_Test_Plugin {

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

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(UnityAudioEffectState *state, float *in_buffer,
                                                                  float *out_buffer, const unsigned int num_samples,
                                                                  const int in_channels, const int out_channels) {
        const auto *data = state->GetEffectData<data_t>();
        const auto p = boundary_parameters(0.5f, 0.5f); // TODO: get from effect parameters

        const float gain = powf(10.0f, data->parameters[param_gain] * 0.05f);
        for (unsigned int n = 0; n < num_samples; n++) {
            // TODO: example computation, change when implementing real functionality
            data->mesh->write_value(0.25f, 0.25f, 0.25f, in_buffer[n * in_channels] * gain);
            data->mesh->update(p, p, p, p, p, p);
            const float output = data->mesh->read_value(0.75f, 0.75f, 0.75f);
            for (int i = 0; i < out_channels; i++) {
                out_buffer[n * out_channels + i] = output;
            }
        }

        return UNITY_AUDIODSP_OK;
    }

} // namespace DWM_Test_Plugin
