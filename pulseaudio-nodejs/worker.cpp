#include <chrono>
#include <thread>
#include <napi.h>
#include <stdio.h>
#include <poll.h>
#include <pulse/pulseaudio.h>
#include <time.h>
#include <math.h>


using namespace Napi;


float get_standard_deviation(float* arr, size_t size){
    //first calculate the average
    float sum = 0;
    for(size_t i = 0; i < size; i++){
        sum += arr[i];
    }
    float avg = sum/size;
    float ssum = 0;
    for(size_t i = 0; i < size; i++){
        float diff = arr[i]-avg;
        ssum += diff*diff;
    }
    return sqrt(ssum/size);
}

const int buff_display_size = 20;
void print_buffer(float* buff, size_t size){
    size /= 4;//because it's in bytes
    //33 in ansi means yellow
    fprintf(stderr,"[ ");
    if(size < buff_display_size){
        //just print the buffer normally
        for(size_t i = 0; i < size; i++){
            fprintf(stderr,"\u001b[33m%f\u001b[0m ",buff[i]);
        }
    }else{
        for(size_t i = 0; i < buff_display_size; i++){
            fprintf(stderr,"\u001b[33m%f\u001b[0m ",buff[i]);
        }
        fprintf(stderr,"...%ld elements... ",size);
        for(size_t i = size-buff_display_size; i < size; i++){
            fprintf(stderr,"\u001b[33m%f\u001b[0m ",buff[i]);
        }
    }
    fprintf(stderr,"]\n");
};



std::thread nativeThread;
ThreadSafeFunction tsfn;

Value RegisterCallback( const CallbackInfo& info ){
    Napi::Env env = info.Env();
    if ( info.Length() < 1 ){
        throw Error::New( env, "usage: registerCallback(function)" );
    }
    else if ( !info[0].IsFunction() )
    {
        throw Error::New( env, "Expected first arg to be function" );
    }
    
    
    tsfn = ThreadSafeFunction::New(
        env,
        info[0].As<Function>(),  // JavaScript function called asynchronously
        "some loop",             // Name
        0,                       // Unlimited queue
        1,                       // Only one thread will use this initially
        []( Napi::Env ) {        // Finalizer used to clean threads up
          nativeThread.join();
        } );
    
    nativeThread = std::thread( [] {
        auto callback = []( Napi::Env env, Function jsCallback, int* value ) {
            // Transform native data into JS data, passing it to the provided
            // `jsCallback` -- the TSFN's JavaScript function.
            jsCallback.Call( {Number::New( env, *value )} );
    
            // We're finished with the data.
            delete value;
        };
//////////////////////////////
//start
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
        
        //not sure if callback goes well with the loop architecture
        /*pa_stream_set_read_callback(
            recording_stream, //pa_stream* p,
            read_cb,//pa_stream_request_cb_t cb,
            NULL//void * userdata 
        );*/
        //main loop
        while(1){
            //break;
            //fprintf(stderr,"just normal loop\n");
            if(pa_stream_get_state(recording_stream) == PA_STREAM_READY){
                //fprintf(stderr,"the stream is ready\n");
                size_t readable_size = pa_stream_readable_size(recording_stream);
                if(readable_size > 0){
                    fprintf(stderr,"readable size is: %zu\n",readable_size);
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
                    fprintf(stderr,"frame size: %zu\n",pa_frame_size(&ss));
                    fprintf(stderr,"timestamp: %ld\n",timestamp);
                    fprintf(stderr,"read size is: %zu\n",read_size);
                    //fprintf(stderr,"stddev: %f\n",get_standard_deviation((float*)data,read_size));
                    //fprintf(stderr,"device name: %s\n",pa_stream_get_device_name(recording_stream));
                    //fprintf(stderr,"device name: %s\n",PA_PROP_MEDIA_FILENAME);
                    print_buffer((float*)data,read_size);
                    pa_stream_drop(recording_stream);
                    // Create new data
                    int* value = new int( clock() );
                    napi_status status = tsfn.BlockingCall( value, callback );
                    if ( status != napi_ok ){
                        // Handle error
                        break;
                    }
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
//end
//////////////////////////////
        /*for ( int i = 0; i < 5; i++ ){
            // Create new data
            int* value = new int( clock() );
    
            // Perform a blocking call
            napi_status status = tsfn.BlockingCall( value, callback );
            if ( status != napi_ok )
            {
                // Handle error
                break;
            }
        
            std::this_thread::sleep_for( std::chrono::seconds( 5 ) );
        }*/
        
        // Release the thread-safe function
        tsfn.Release();
    } );
    return Boolean::New(env, true);
}

Object Init(Env env, Object exports) {
    exports.Set( "registerCallback", Function::New( env, RegisterCallback ) );
    return exports;
}

NODE_API_MODULE(testaddon, Init)
