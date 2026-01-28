/* encoder_plugin.cpp - HIGH QUALITY MODE (64K) Memory-Safe Multi-Decoder */
#include <string>
#include <vector>
#include <cstring>
#include <mutex>
#include <map>
#include <memory>
#include "../../src/nvgt_plugin.h"
#include "scriptarray.h"
#include "opus/include/opus.h"
using namespace std;
// Encoder
struct EncoderInstance {
OpusEncoder* encoder;
vector<float> input_buffer;
string last_compressed_data;
bool has_compressed;
int bitrate;
int complexity;
// Buffering
float min_buffer_sec;
bool is_buffering;
// Volume
float volume; // Input gain 0.0 - 1.0 (Derived from 0-100)
EncoderInstance() : encoder(nullptr), has_compressed(false), bitrate(24000), complexity(6), min_buffer_sec(0.0f), is_buffering(false), volume(1.0f) {}
~EncoderInstance() {
if (encoder) {
opus_encoder_destroy(encoder);
encoder = nullptr;
}
}
};
map<int, unique_ptr<EncoderInstance>> g_encoders;
mutex g_encoders_lock;
int g_next_encoder_id = 1;
// Global Volume
float g_master_volume = 1.0f;
// Legacy global encoder support (lazy initialized)
EncoderInstance* GetLegacyEncoder() {
// We use ID 0 for the legacy global encoder
lock_guard<mutex> guard(g_encoders_lock);
auto it = g_encoders.find(0);
if (it == g_encoders.end()) {
auto instance = make_unique<EncoderInstance>();
int err;
instance->encoder = opus_encoder_create(48000, 1, OPUS_APPLICATION_VOIP, &err);
if (err == OPUS_OK) {
opus_encoder_ctl(instance->encoder, OPUS_SET_VBR(1));
opus_encoder_ctl(instance->encoder, OPUS_SET_BITRATE(instance->bitrate));
opus_encoder_ctl(instance->encoder, OPUS_SET_COMPLEXITY(instance->complexity));
opus_encoder_ctl(instance->encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
opus_encoder_ctl(instance->encoder, OPUS_SET_INBAND_FEC(1));
opus_encoder_ctl(instance->encoder, OPUS_SET_DTX(1));
opus_encoder_ctl(instance->encoder, OPUS_SET_PACKET_LOSS_PERC(5));
g_encoders[0] = std::move(instance);
return g_encoders[0].get();
}
return nullptr;
}
return it->second.get();
}
// Multi-decoder
struct DecoderInstance {
OpusDecoder* decoder;
vector<float> pcm_buffer; // Jitter buffer
float min_buffer_sec;
bool is_buffering;
// Volume
float volume; // Stream volume 0.0 - 1.0 (Derived from 0-100)
DecoderInstance() : decoder(nullptr), min_buffer_sec(0.0f), is_buffering(true), volume(1.0f) {}
~DecoderInstance() {
if (decoder) {
opus_decoder_destroy(decoder);
decoder = nullptr;
}
}
};
map<int, unique_ptr<DecoderInstance>> g_decoders;
mutex g_decoders_lock;
int g_next_decoder_id = 1;
// Audio constants
const int SAMPLE_RATE = 48000;
const int CHANNELS = 1;
const int FRAME_SIZE = 960;
const int MAX_PACKET = 2500;
// Opus initialization
void InitOpus() {
// No global init needed anymore, handled per instance
}
// Encoder Implementation
int vc_create_encoder(int bitrate, int complexity) {
lock_guard<mutex> guard(g_encoders_lock);
int id = g_next_encoder_id++;
int err;
OpusEncoder* enc = opus_encoder_create(SAMPLE_RATE, CHANNELS, OPUS_APPLICATION_VOIP, &err);
if (err != OPUS_OK || !enc) return -1;
auto instance = make_unique<EncoderInstance>();
instance->encoder = enc;
instance->bitrate = bitrate;
instance->complexity = complexity;
// Default: No buffering unless requested
instance->min_buffer_sec = 0.0f;
instance->is_buffering = false;
opus_encoder_ctl(enc, OPUS_SET_VBR(1));
opus_encoder_ctl(enc, OPUS_SET_BITRATE(bitrate));
opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(complexity));
opus_encoder_ctl(enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
opus_encoder_ctl(enc, OPUS_SET_INBAND_FEC(1));
opus_encoder_ctl(enc, OPUS_SET_DTX(1));
opus_encoder_ctl(enc, OPUS_SET_PACKET_LOSS_PERC(5));
g_encoders[id] = std::move(instance);
return id;
}
int vc_destroy_encoder(int id) {
lock_guard<mutex> guard(g_encoders_lock);
auto it = g_encoders.find(id);
if (it == g_encoders.end()) return -1;
g_encoders.erase(it);
return 0;
}
void vc_set_encoder_buffer(int id, float seconds) {
lock_guard<mutex> guard(g_encoders_lock);
auto it = g_encoders.find(id);
if (it != g_encoders.end()) {
it->second->min_buffer_sec = seconds;
if (seconds > 0) it->second->is_buffering = true;
}
}
// Encoder Volume Control
void vc_set_encoder_input_volume(int id, float volume_percent) {
if (volume_percent < 0) volume_percent = 0;
if (volume_percent > 100) volume_percent = 100; // Normalize? Or allow amplification?
// User request implication: 0-100 standard.
float vol_float = volume_percent / 100.0f;
lock_guard<mutex> guard(g_encoders_lock);
auto it = g_encoders.find(id);
if (it != g_encoders.end()) {
it->second->volume = vol_float;
}
}
void vc_feed_encoder(int id, CScriptArray* arr) {
lock_guard<mutex> guard(g_encoders_lock);
auto it = g_encoders.find(id);
if (it == g_encoders.end()) return;
EncoderInstance* instance = it->second.get();
if (!arr || arr->GetSize() == 0) return;
float* data = (float*)arr->GetBuffer();
int count = arr->GetSize();
// Apply Input Volume if needed
if (instance->volume != 1.0f) {
// We should copy data if we are going to modify it?
// Actually, we copy into the input_buffer anyway.
// We can multiply ON INSERT.
size_t current_size = instance->input_buffer.size();
instance->input_buffer.resize(current_size + count);
float* dest = instance->input_buffer.data() + current_size;
float vol = instance->volume;
for(int i=0; i<count; i++) {
dest[i] = data[i] * vol;
}
} else {
// Fast path
instance->input_buffer.insert(instance->input_buffer.end(), data, data + count);
}
// Safety limit: Max 10 seconds to prevent infinite piling
if (instance->input_buffer.size() > SAMPLE_RATE * 10) {
instance->input_buffer.erase(instance->input_buffer.begin(),
instance->input_buffer.end() - SAMPLE_RATE * 2);
}
}
string vc_get_encoded_packet(int id) {
lock_guard<mutex> guard(g_encoders_lock);
auto it = g_encoders.find(id);
if (it == g_encoders.end()) return "";
EncoderInstance* instance = it->second.get();
if (!instance->encoder) return "";
int samples_needed = FRAME_SIZE * CHANNELS;
// Check buffering
if (instance->is_buffering) {
float current_sec = (float)instance->input_buffer.size() / (float)SAMPLE_RATE;
if (current_sec >= instance->min_buffer_sec) {
instance->is_buffering = false;
} else {
return ""; // Still buffering
}
} else {
// If buffer runs dry and we have a requirement, start buffering again?
// User requirement "biriktiren yapı". If it runs dry, we likely want to catch next burst.
// Let's say if buffer < samples_needed, we just return empty.
// If we want Re-Buffering (like jitter buffer), we should check that.
if (instance->min_buffer_sec > 0 && instance->input_buffer.size() == 0) {
instance->is_buffering = true;
return "";
}
}
if ((int)instance->input_buffer.size() >= samples_needed) {
unsigned char out_buf[MAX_PACKET];
int bytes = opus_encode_float(instance->encoder, instance->input_buffer.data(), FRAME_SIZE, out_buf, MAX_PACKET);
instance->input_buffer.erase(instance->input_buffer.begin(), instance->input_buffer.begin() + samples_needed);
if (bytes > 0) {
return string((char*)out_buf, bytes);
}
}
return "";
}
// Wrapper for backward compatibility with my own previous iteration or simplify usage
string vc_encode(int id, CScriptArray* arr) {
vc_feed_encoder(id, arr);
return vc_get_encoded_packet(id);
}
// Global Legacy Functions (mapped to ID 0)
int vc_compress_internal(CScriptArray* arr) {
EncoderInstance* instance = GetLegacyEncoder();
if (!instance) return 0;
lock_guard<mutex> guard(g_encoders_lock); // Lock for using the instance
instance->has_compressed = false;
instance->last_compressed_data.clear();
if (!arr || arr->GetSize() == 0) return 0;
float* data = (float*)arr->GetBuffer();
int count = arr->GetSize();
instance->input_buffer.insert(instance->input_buffer.end(), data, data + count);
int samples_needed = FRAME_SIZE * CHANNELS;
if (instance->input_buffer.size() > samples_needed * 4) {
instance->input_buffer.erase(instance->input_buffer.begin(),
instance->input_buffer.end() - samples_needed);
}
if ((int)instance->input_buffer.size() >= samples_needed) {
unsigned char out_buf[MAX_PACKET];
int bytes = opus_encode_float(instance->encoder, instance->input_buffer.data(), FRAME_SIZE, out_buf, MAX_PACKET);
instance->input_buffer.erase(instance->input_buffer.begin(), instance->input_buffer.begin() + samples_needed);
if (bytes > 0) {
instance->last_compressed_data.assign((char*)out_buf, bytes);
instance->has_compressed = true;
return bytes;
}
}
return 0;
}
string vc_get_data() {
EncoderInstance* instance = GetLegacyEncoder();
if (!instance) return "";
lock_guard<mutex> guard(g_encoders_lock);
if (instance->has_compressed) {
instance->has_compressed = false;
return std::move(instance->last_compressed_data);
}
return "";
}
// Decoder management
int vc_create_decoder() {
lock_guard<mutex> guard(g_decoders_lock);
int decoder_id = g_next_decoder_id++;
int err;
OpusDecoder* decoder = opus_decoder_create(SAMPLE_RATE, CHANNELS, &err);
if (err != OPUS_OK || !decoder) return -1;
auto instance = make_unique<DecoderInstance>();
instance->decoder = decoder;
g_decoders[decoder_id] = std::move(instance);
return decoder_id;
}
int vc_destroy_decoder(int stream_id) {
lock_guard<mutex> guard(g_decoders_lock);
auto it = g_decoders.find(stream_id);
if (it == g_decoders.end()) return -1;
// unique_ptr will destroy decoder automatically
g_decoders.erase(it);
return 0;
}
// Decoder
// Decoder Volume Control
void vc_set_master_volume(float volume_percent) {
if (volume_percent < 0) volume_percent = 0;
float vol_float = volume_percent / 100.0f;
// Global variable, strictly doesn't need lock if atomic float read/write,
// but let's assume single thread config or accept minor race.
// For correctness with other threads:
// We don't have a global lock for this variable alone.
// Let's just set it. Float write is usually atomic enough for audio volume.
g_master_volume = vol_float;
}
void vc_set_stream_volume(int stream_id, float volume_percent) {
if (volume_percent < 0) volume_percent = 0;
float vol_float = volume_percent / 100.0f;
lock_guard<mutex> guard(g_decoders_lock);
auto it = g_decoders.find(stream_id);
if (it != g_decoders.end()) {
it->second->volume = vol_float;
}
}
void vc_set_decoder_buffer(int stream_id, float seconds) {
lock_guard<mutex> guard(g_decoders_lock);
auto it = g_decoders.find(stream_id);
if (it != g_decoders.end()) {
it->second->min_buffer_sec = seconds;
it->second->is_buffering = true;
}
}
int vc_decompress_internal(int stream_id, const string& data) {
lock_guard<mutex> guard(g_decoders_lock);
auto it = g_decoders.find(stream_id);
if (it == g_decoders.end()) return -1;
DecoderInstance* instance = it->second.get();
if (data.empty() || !instance->decoder) return 0;
float out_pcm[FRAME_SIZE * CHANNELS];
int frames = opus_decode_float(instance->decoder,
(const unsigned char*)data.c_str(),
data.length(),
out_pcm,
FRAME_SIZE,
0);
if (frames > 0) {
int sample_count = frames * CHANNELS;
// Append to buffer instead of just overwriting last_pcm_data
instance->pcm_buffer.insert(instance->pcm_buffer.end(), out_pcm, out_pcm + sample_count);
// Check buffering state
if (instance->is_buffering) {
float current_sec = (float)instance->pcm_buffer.size() / (float)SAMPLE_RATE;
if (current_sec >= instance->min_buffer_sec) {
instance->is_buffering = false;
}
}
return sample_count;
}
return 0;
}
int vc_get_pcm_size(int stream_id) {
lock_guard<mutex> guard(g_decoders_lock);
auto it = g_decoders.find(stream_id);
if (it == g_decoders.end()) return -1;
DecoderInstance* inst = it->second.get();
if (inst->is_buffering) return 0;
return (int)inst->pcm_buffer.size();
}
void vc_get_pcm(int stream_id, CScriptArray* out_arr) {
lock_guard<mutex> guard(g_decoders_lock);
auto it = g_decoders.find(stream_id);
if (it == g_decoders.end() || !out_arr) return;
DecoderInstance* instance = it->second.get();
if (instance->is_buffering) return;
if (instance->pcm_buffer.empty()) {
// Buffer underrun, start buffering again if we require a min buffer
if (instance->min_buffer_sec > 0) {
instance->is_buffering = true;
}
return;
}
int size = (int)instance->pcm_buffer.size();
int arr_size = (int)out_arr->GetSize();
int copy_size = size;
if (arr_size < size) copy_size = arr_size;
if (copy_size > 0) {
float* dest = (float*)out_arr->GetBuffer();
// Apply Volume: Stream Volume * Master Volume
float final_gain = instance->volume * g_master_volume;
if (final_gain == 1.0f) {
// Fast path
if (dest) memcpy(dest, instance->pcm_buffer.data(), copy_size * sizeof(float));
} else {
// Amplification path
const float* src = instance->pcm_buffer.data();
if (dest) {
for(int i=0; i<copy_size; i++) {
dest[i] = src[i] * final_gain;
}
}
}
// Remove consumed data
instance->pcm_buffer.erase(instance->pcm_buffer.begin(), instance->pcm_buffer.begin() + copy_size);
}
}
// Utilities
int vc_get_decoder_count() {
lock_guard<mutex> guard(g_decoders_lock);
return (int)g_decoders.size();
}
// Backward compatibility
const int DEFAULT_STREAM_ID = 0;
int vc_decompress_compat(const string& data) {
lock_guard<mutex> guard(g_decoders_lock);
if (g_decoders.find(DEFAULT_STREAM_ID) == g_decoders.end()) {
int err;
OpusDecoder* decoder = opus_decoder_create(SAMPLE_RATE, CHANNELS, &err);
if (err == OPUS_OK && decoder) {
auto instance = make_unique<DecoderInstance>();
instance->decoder = decoder;
g_decoders[DEFAULT_STREAM_ID] = std::move(instance);
}
}
return vc_decompress_internal(DEFAULT_STREAM_ID, data);
}
int vc_get_pcm_size_compat() {
return vc_get_pcm_size(DEFAULT_STREAM_ID);
}
void vc_get_pcm_compat(CScriptArray* out_arr) {
vc_get_pcm(DEFAULT_STREAM_ID, out_arr);
}
// Plugin entry
plugin_main(nvgt_plugin_shared* shared) {
prepare_plugin(shared);
InitOpus();
// Encoder New
shared->script_engine->RegisterGlobalFunction("int vc_create_encoder(int bitrate = 24000, int complexity = 6)", asFUNCTION(vc_create_encoder), asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("int vc_destroy_encoder(int)", asFUNCTION(vc_destroy_encoder), asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("string vc_encode(int, float[]@)", asFUNCTION(vc_encode), asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void vc_feed_encoder(int, float[]@)", asFUNCTION(vc_feed_encoder), asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("string vc_get_encoded_packet(int)", asFUNCTION(vc_get_encoded_packet), asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void vc_set_encoder_buffer(int, float)", asFUNCTION(vc_set_encoder_buffer), asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void vc_set_encoder_input_volume(int, float)", asFUNCTION(vc_set_encoder_input_volume), asCALL_CDECL);
// Encoder Legacy
shared->script_engine->RegisterGlobalFunction("int vc_compress(float[]@ pcm)", asFUNCTION(vc_compress_internal), asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("string vc_get_data()", asFUNCTION(vc_get_data), asCALL_CDECL);
// Decoder management
shared->script_engine->RegisterGlobalFunction("int vc_create_decoder()", asFUNCTION(vc_create_decoder), asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("int vc_destroy_decoder(int stream_id)", asFUNCTION(vc_destroy_decoder), asCALL_CDECL);
// Multi-decoder
shared->script_engine->RegisterGlobalFunction("int vc_decompress(int stream_id, const string &in data)", asFUNCTION(vc_decompress_internal), asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("int vc_get_pcm_size(int stream_id)", asFUNCTION(vc_get_pcm_size), asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void vc_get_pcm(int stream_id, float[]@ pcm)", asFUNCTION(vc_get_pcm), asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void vc_set_decoder_buffer(int stream_id, float seconds)", asFUNCTION(vc_set_decoder_buffer), asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void vc_set_master_volume(float)", asFUNCTION(vc_set_master_volume), asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void vc_set_stream_volume(int, float)", asFUNCTION(vc_set_stream_volume), asCALL_CDECL);
// Backward compatibility
shared->script_engine->RegisterGlobalFunction("int vc_decompress(const string &in data)", asFUNCTION(vc_decompress_compat), asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("int vc_get_pcm_size()", asFUNCTION(vc_get_pcm_size_compat), asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("void vc_get_pcm(float[]@ pcm)", asFUNCTION(vc_get_pcm_compat), asCALL_CDECL);
// Utility
shared->script_engine->RegisterGlobalFunction("int vc_get_decoder_count()", asFUNCTION(vc_get_decoder_count), asCALL_CDECL);
return true;
}