// ReSharper disable CppParameterMayBeConstPtrOrRef
// ReSharper disable CppDFAConstantFunctionResult
#include <atomic>
#include "AudioPluginUtil.h"
#include "binauraliser.h"
#include "dwm.h"
#include "plugin_config.h"

namespace HRTF {
    enum Param { P_Gain, P_NUM };

    struct EffectData {
        float p[P_NUM]{};
        void *binauraliser = nullptr;
    };

    int InternalRegisterEffectDefinition(UnityAudioEffectDefinition &definition) {
        definition.paramdefs = new UnityAudioParameterDefinition[P_NUM];
        AudioPluginUtil::RegisterParameter(definition, "Gain", "dB", -100.0f, 100.0f,
                                           0.0f, 1.0f, 1.0f, P_Gain,
                                           "Overall gain applied");

        definition.flags |= UnityAudioEffectDefinitionFlags_IsSpatializer;
        //definition.flags |= UnityAudioEffectDefinitionFlags_AppliesDistanceAttenuation;
        return P_NUM;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK
    CreateCallback(UnityAudioEffectState *state) {
        auto *effectdata = new EffectData;
        binauraliser_create(&effectdata->binauraliser);
        binauraliser_init(effectdata->binauraliser, 16000);
        binauraliser_setEnableHRIRsDiffuseEQ(effectdata->binauraliser, 1);
        binauraliser_setEnableRotation(effectdata->binauraliser, 1);
        binauraliser_initCodec(effectdata->binauraliser);
        AudioPluginUtil::InitParametersFromDefinitions(
            InternalRegisterEffectDefinition, effectdata->p);
        state->effectdata = effectdata;
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK
    ReleaseCallback(UnityAudioEffectState *state) {
        auto *effectdata = state->GetEffectData<EffectData>();
        binauraliser_destroy(&effectdata->binauraliser);
        delete effectdata;
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK SetFloatParameterCallback(
        UnityAudioEffectState *state, const int index, const float value) {
        auto *effectdata = state->GetEffectData<EffectData>();
        if (index >= P_NUM)
            return UNITY_AUDIODSP_ERR_UNSUPPORTED;
        effectdata->p[index] = value;
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK GetFloatParameterCallback(
        UnityAudioEffectState *state, int index, float *value, char *valuestr) {
        const auto *effectdata = state->GetEffectData<EffectData>();
        if (value != nullptr)
            *value = effectdata->p[index];
        if (valuestr != nullptr)
            valuestr[0] = 0;
        return UNITY_AUDIODSP_OK;
    }

    int UNITY_AUDIODSP_CALLBACK GetFloatBufferCallback(UnityAudioEffectState *state,
                                                       const char *name,
                                                       float *buffer,
                                                       int numsamples) {
        auto *effectdata = state->GetEffectData<EffectData>();
        memset(buffer, 0, sizeof(float) * numsamples);

        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK
    ProcessCallback(UnityAudioEffectState *state, float *inbuffer, float *outbuffer,
                    unsigned int length, int inchannels, int outchannels) {
        auto *data = state->GetEffectData<EffectData>();

        auto *L = state->spatializerdata->listenermatrix;
        auto *S = state->spatializerdata->sourcematrix;

        float sourcepos_x = S[12];
        float sourcepos_y = S[13];
        float sourcepos_z = S[14];

        float listenerpos_x = -(L[0] * L[12] + L[1] * L[13] + L[2] * L[14]);
        float listenerpos_y = -(L[4] * L[12] + L[5] * L[13] + L[6] * L[14]);
        float listenerpos_z = -(L[8] * L[12] + L[9] * L[13] + L[10] * L[14]);

        const float delta_x = - listenerpos_x + sourcepos_x;
        const float delta_y = - listenerpos_y + sourcepos_y;
        const float delta_z = - listenerpos_z + sourcepos_z;

        const float delta_r_x = L[0] * delta_x + L[4] * delta_y + L[8] * delta_z;
        const float delta_r_y = L[1] * delta_x + L[5] * delta_y + L[9] * delta_z;
        const float delta_r_z = L[2] * delta_x + L[6] * delta_y + L[10] * delta_z;
        binauraliser_setSourceAzi_deg(data->binauraliser, 0,
                                      -std::atan2(delta_r_x, delta_r_z) / AudioPluginUtil::kPI * 180.0f);
        binauraliser_setSourceElev_deg(
                data->binauraliser, 0,
                std::atan2(delta_r_y, std::sqrt(delta_r_x * delta_r_x + delta_r_z * delta_r_z)) /
                        AudioPluginUtil::kPI * 180.0f);

        float temp_1[128];
        float temp_2[128];
        float temp_3[128];

        const float gain = powf(10.0f, data->p[P_Gain] * 0.05f);
        for (unsigned int n = 0; n < length; n++) {
            temp_1[n] = inbuffer[n * inchannels] * gain;
        }

        const float *const_temp1 = temp_1;

        float *outputs[2] = {temp_2, temp_3};
        binauraliser_process(data->binauraliser, &const_temp1, outputs, 1, 2, 128);

        for (unsigned int n = 0; n < length; n++) {
            outbuffer[n * outchannels + 0] = temp_2[n];
            outbuffer[n * outchannels + 1] = temp_3[n];
            for (int i = 2; i < outchannels; i++) {
                outbuffer[n * outchannels + i] = 0;
            }
        }

        return UNITY_AUDIODSP_OK;
    }
}; // namespace Plugin


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

    enum param_t {
        param_raw_gain,
        param_hrtf_gain,
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
    };

    int InternalRegisterEffectDefinition(UnityAudioEffectDefinition &definition) {
        definition.paramdefs = new UnityAudioParameterDefinition[param_num];
        AudioPluginUtil::RegisterParameter(definition, "Raw Gain", "dB", //
                                           -100.0f, 100.0f, 0.0f, //
                                           1.0f, 1.0f, //
                                           param_raw_gain, "Overall gain applied");

        AudioPluginUtil::RegisterParameter(definition, "HRTF Gain", "dB", //
                                   -100.0f, 100.0f, 0.0f, //
                                   1.0f, 1.0f, //
                                   param_hrtf_gain, "Overall gain applied");

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

        const float raw_gain = powf(10.0f, data->parameters[param_raw_gain] * 0.05f);
        const float hrtf_gain = powf(10.0f, data->parameters[param_hrtf_gain] * 0.05f);
        for (unsigned int n = 0; n < num_samples; n++) {
            for (auto &[p_x, p_y, p_z, buffer]: dwm_source_data) {
                data->mesh->write_value(p_x, p_y, p_z, buffer[n] * raw_gain);
                buffer[n] = 0;
            }
            data->mesh->update(p_xp, p_xn, p_yp, p_yn, p_zp, p_zn);
#ifdef MONO_LISTEN
            const float v = data->mesh->read_value(listen_l_x, listen_l_y, listen_l_z);
            out_buffer[n * out_channels + 0] = v + in_buffer[n * in_channels + 0] * hrtf_gain;
            out_buffer[n * out_channels + 1] = v + in_buffer[n * in_channels + 1] * hrtf_gain;
#else
            out_buffer[n * out_channels + 0] = data->mesh->read_value(listen_l_x, listen_l_y, listen_l_z) + in_buffer[n * in_channels + 0] * hrtf_gain;
            out_buffer[n * out_channels + 1] = data->mesh->read_value(listen_r_x, listen_r_y, listen_r_z) + in_buffer[n * in_channels + 1] * hrtf_gain;
#endif
            for (int i = 2; i < out_channels; i++) {
                out_buffer[n * out_channels + i] = 0.0f;
            }
        }

        return UNITY_AUDIODSP_OK;
    }

} // namespace DWM_Mesh_Simulation
