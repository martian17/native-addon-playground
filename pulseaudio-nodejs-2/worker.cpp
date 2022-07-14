#include <thread>
#include <napi.h>
#include <stdio.h>
#include <pulse/pulseaudio.h>
#include <vector>


using namespace std;
using namespace Napi;

std::thread nativeThread;

void mainLoop(Napi::Env env, ThreadSafeFunction tsfn){
    fprintf(stderr,"this program will monitor the output of audio devices\n");
    fprintf(stderr,"the code closely refrects the one you can find at http://ysflight.in.coocan.jp/programming/audio/pulseAudioSample/e.html, which I used as a tutorial\n");
    
    
    pa_mainloop* mainloop = pa_mainloop_new();
    if(mainloop == NULL){
        fprintf(stderr,"pa_mainloop_new: Cannot create main loop\n");
        exit(1);
    }
    pa_context *ctx = pa_context_new(pa_mainloop_get_api(mainloop),"record_ctx");
    if(ctx == NULL){
        fprintf(stderr,"pa_context_new: Cannot create context\n");
        exit(1);
    }
    
    pa_context_connect(ctx,NULL,(pa_context_flags_t)0,NULL);
    
    //wait until the loop is ready
    while(true)
    {
        pa_mainloop_iterate(mainloop,0,NULL);
        if(pa_context_get_state(ctx)==PA_CONTEXT_READY){
            break;
        }
    }
    //YsPulseAudioWaitForConnectionEstablished(ctx,mainloop,5);
    fprintf(stderr,"ready fired!!\n");
    
    
    //this settings will mirror the client side javascript
    //PA_SAMPLE_FLOAT32LE  32 Bit IEEE floating point, little endian (PC), range -1.0 to 1.0
    const pa_sample_spec ss = {
        .format = PA_SAMPLE_FLOAT32LE,
        .rate = 44100,
        .channels = 2
    };
    
    pa_stream *recording_stream=pa_stream_new(ctx,"record_stream",&ss,NULL);
    if(recording_stream == NULL){
        fprintf(stderr,"pa_stream_new: Cannot create stream\n");
        exit(1);
    }
    
    pa_stream_connect_record(
        recording_stream,//pa_stream * 	s,
        NULL,//const char * 	dev,
        NULL,//const pa_buffer_attr * 	attr,
        PA_STREAM_NOFLAGS //pa_stream_flags_t 	flags 
    );
    
    fprintf(stderr,"mainloop, context, and stream created. Entering main loop\n");
    
    //main loop
    while(1){
        //break;
        //fprintf(stderr,"just normal loop\n");
        if(pa_stream_get_state(recording_stream) == PA_STREAM_READY){
            //fprintf(stderr,"the stream is ready\n");
            size_t readable_size = pa_stream_readable_size(recording_stream);
            if(readable_size > 0){
                //fprintf(stderr,"readable size is: %zu\n",readable_size);
                pa_usec_t timestamp;
                pa_stream_get_time(
                    recording_stream,
                    &timestamp
                );
                const void *data;
                size_t read_size;
                pa_stream_peek(
                    recording_stream,
                    &data,
                    &read_size
                );
                //fprintf(stderr,"frame size: %zu\n",pa_frame_size(&ss));
                //fprintf(stderr,"timestamp: %ld\n",timestamp);
                fprintf(stderr,"[c++] read size is: %zu\n",read_size);
                //fprintf(stderr,"stddev: %f\n",get_standard_deviation((float*)data,read_size));
                //fprintf(stderr,"device name: %s\n",pa_stream_get_device_name(recording_stream));
                //fprintf(stderr,"device name: %s\n",PA_PROP_MEDIA_FILENAME);
                //print_buffer((float*)data,read_size);
                // Create new data
                /*auto databuff = Buffer<void>::Copy(env, data, read_size);
                napi_status status = tsfn.BlockingCall(
                    &databuff, 
                    [](Napi::Env env, Function jsCallback, Buffer<void> buff){
                        jsCallback.Call( {buff} );
                    }
                );*/
                //int* value = new int( 42 );
                //Buffer<void> buff = Buffer<void>::Copy(env, data, read_size);
                //Buffer<void>* buffptr = new Buffer<void>;
                vector<uint8_t>* vec = new vector<uint8_t>();
                vec->insert(vec->end(),&((uint8_t*)data)[0],&((uint8_t*)data)[read_size]);
                napi_status status = tsfn.BlockingCall( vec, []( Napi::Env env, Function jsCallback, vector<uint8_t>* vec ) {
                    // Transform native data into JS data, passing it to the provided
                    // `jsCallback` -- the TSFN's JavaScript function.
                    //jsCallback.Call( {Number::New( env, *value )} );
                    jsCallback.Call( {Buffer<void>::Copy(env, &(vec->at(0)), vec->size())} );
                    // We're finished with the data.
                    delete vec;
                } );
                if ( status != napi_ok ){
                    // Handle error
                    break;
                }
                pa_stream_drop(recording_stream);
            }
        }
        int itr_result = pa_mainloop_iterate(mainloop,0,NULL);
        if(itr_result < 0)break;
        //fprintf(stderr, "itr: %d\n",itr_result);
    }
    
    //free everything!
    pa_stream_disconnect(recording_stream);
    pa_stream_unref(recording_stream);
    pa_context_disconnect(ctx);
    pa_context_unref(ctx);
    pa_mainloop_free(mainloop);
    
    fprintf(stderr,"successfully exiting\n");
}




Value RegisterCallback( const CallbackInfo& info ){
    Napi::Env env = info.Env();
    if ( info.Length() < 1 ){
        throw Error::New( env, "usage: registerCallback(function)" );
    }
    else if ( !info[0].IsFunction() )
    {
        throw Error::New( env, "Expected first arg to be function" );
    }
    
    
    auto tsfn = ThreadSafeFunction::New(
        env,
        info[0].As<Function>(),  // JavaScript function called asynchronously
        "some loop",             // Name
        0,                       // Unlimited queue
        1,                       // Only one thread will use this initially
        []( Napi::Env ) {        // Finalizer used to clean threads up
          nativeThread.join();
        } );
    
    nativeThread = std::thread( [env,tsfn] {
        mainLoop(env,tsfn);
        // Release the thread-safe function
        tsfn.Release();
    } );
    return Boolean::New(env, true);
}




Object Init(Env env, Object exports) {
    exports.Set( "onChunk", Function::New( env, RegisterCallback ) );
    return exports;
}

NODE_API_MODULE(testaddon, Init)


