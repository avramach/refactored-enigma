#ifndef _RDK_GST_AV_DECODER_H_
#define _RDK_GST_AV_DECODER_H_

#include <stdio.h>
#include "media/IVideoDecoder.h"
#include "../source/adapters/H264Utils.h"
#include "RDKAACUtils.h"
#include <glib.h>

namespace media 
{
   class RDKGstAVDecoder: public IVideoDecoder,public H264ParserSink
   {
      public:
         struct EsBuffer
         {
            char * data;
            uint size;
            int64_t pts;
            PayloadType type;
         };
         enum DecState
         {
            kStateStopped,
            kStatePreroll,
            kStatePause, 
            kStatePaused, 
            kStatePlay,
            kStatePlaying
         };

         RDKGstAVDecoder(MediaComponentFactory* factory);
         virtual ~RDKGstAVDecoder();

         /*---------------------------------------*/
         /* Overridden IVideoDecoder functions*/
         /*---------------------------------------*/
         virtual bool GetCapabilities(DecoderObjectFactory* pFactory, Capabilities& capabilitiesToSet);
         virtual bool InitializeDecompressor(const VideoMetaData& metaData, DecoderConfig& configToSet);
         virtual bool ConsumeData(StreamPayload* pPayload, bool interleaved = true);
         virtual void GetQOSData(QOSData & qosDataToSet);
         virtual void NotifyEOF();
         bool HandleStreamSwitch(const VideoMetaData & metadata);
         virtual TimeStamp GetVideoPosition();
         virtual void Pause();
         virtual bool Resume();
         virtual void Seek(TimeStamp position);
         virtual void SetBufferTime(int32_t bufferTime);

         /*---------------------------------------*/
         virtual bool SPSPPSSink(uint8_t* , int32_t , bool ,bool firstChunk = false, bool lastChunk = false);
         virtual bool H264SampleSink(TimeStamp , uint8_t* ,int32_t );
         /*---------------------------------------*/
         gboolean  CheckEosReceived(void){return mEosRecieved;}
         GstElement * GetAppsrc(PayloadType type)
         {
            if(type == kPTVideo)
            {
               return mVideoAppSrc;
            }
            if(type == kPTAudio)
            {
               return mAudioAppSrc;
            }
         }
         void SetAppsrc(GstElement * appsrc,PayloadType type)
         {
            if(type == kPTVideo)
            {
               mVideoAppSrc = appsrc;
            }
            if(type == kPTAudio)
            {
               mAudioAppSrc = appsrc;
            }
            gst_object_ref (appsrc);
         }
         void SetDynAppsrc(GstElement * elem){mDynAppsrc = elem;}
         void GetDecoderState(void){return mDecoderState;}
         void SetDecoderState(DecState state){mDecoderState = state;}

         GstElement * GetPipeline(void){return mPlaybin;}
         void NotifyPipelineStateChange(GstState state);
      private:

         IVideoDecoder::Notifier * mNotifier;
         VideoMetaData mStreamMetadata;

         GstElement *mPlaybin;
         GstElement *mDynAppsrc;
         GstElement *mVideoAppSrc;
         GstElement *mAudioAppSrc;
         GstBus *mBus;
         gboolean mEosRecieved;
         DecState mDecoderState;
         GCond              mStateCondition;
         GMutex             mPipelineMutex;
         GMainContext       *mMainLoopThreadContext;
         GThread            *mMainLoopThread;
         GSource            *mBusWatch;
         GMainLoop          *mLoop;

         int64_t mCurrentTimeStampVideo;
         int64_t mCurrentTimeStampAudio;
         int64_t mSeekTimeStamp;
         H264Utils mH264Utils;
         RDKAACUtils mAacUtils;

         void InitializeGstPipeline(void);
         static void PlaybinFoundSource(GObject * object, GObject * orig, GParamSpec * pspec, void * user);
         static gboolean GstBusCallback(GstBus *bus, GstMessage *msg, gpointer data);
         static void PipelineElementAdded(GstBin *bin, GstElement *element, gpointer  data);
         GstBuffer * CreateGstBuffer( EsBuffer * buffer);
         char *GetPipelineStateStr (GstState state);
         void SetAppsrcCaps(PayloadType type);
         void SendEOF(void);
         gboolean ChangePipelineState(DecState state);
         void SetupPreRollState(void);
         void FlushPipeline(void);

         bool ConsumeVideoPayload(StreamPayload* pPayload);
         bool ConsumeAudioPayload(StreamPayload* pPayload);
         void PushtoDecoder(EsBuffer * newBuffer);
         void PushParsedH264Buffer(uint8_t * data, int32_t len);
         void PushParsedAACBuffer(uint8_t * data, int32_t len);
         bool CanConsumeData(PayloadType type, uint32_t length);
   };

} 

#endif // _RDK_GST_AV_DECODER_H
