#include "common_define.h"
#include "transcoder.h"

static void* TranscodeThread(void* args)
{
    Transcoder* transcoder = (Transcoder*)args;

    int ret = -1;

    while (! transcoder->quit_)
    {
        while (transcoder->packet_queue_.empty())
        {
            pthread_cond_wait(&packet_queue_cond_, &packet_queue_mutex_);
        }

        MediaPacket packet;

        {
            pthread_mutex_lock(&packet_queue_mutex_);
            packet = transcoder->packet_queue_[0];
            pthread_mutex_unlock(&packet_queue_mutex_);
        }

        if (packet.IsVideo())
        {
            if (packet.IsHeader())
            {
                ret = video_decoder.Init();

                if (ret != 0)
                {
                    break;
                }
            }
            else
            {
                ret = video_decoder.Decode();

                if (ret != 0)
                {
                    continue;
                }

                for (size_t index = 0; index != scale_vec_.size(); ++index)
                {
                    ret = scale_vec_[index]->Scale();

                    if (ret != 0)
                    {
                        continue;
                    }

                    encoder_vec_[index]->Encode();
                }
            }
        }
    }

    return NULL;
}

Transcoder::Transcoder()
    :
    pthread_id_(-1), 
    quit_(false)
{
}

Transcoder::~Transcoder()
{
}

int Transcoder::StartTransThread()
{
    if (pthread_id_ != -1)
    {
        cout << LMSG << "thread already created and running" << endl;
        return -1;
    }

    int ret = pthread_cond_init(&packet_queue_cond_, NULL);
    if (ret != 0)
    {
        cout << LMSG << "pthread_cond_init failed:" << strerror(errno) << endl;
        return ret;
    }

    ret = pthread_mutex_init(&packet_queue_mutex_);
    if (ret != 0)
    {
        cout << LMSG << "pthread_mutex_init failed:" << strerror(errno) << endl;
        return ret;
    }

    //int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);
    ret = pthread_create(&pthread_id_, NULL, TranscodeThread, (void*)this);

    if (ret != 0)
    {
        cout << LMSG << "pthread_create err:" << strerror(errno) << endl;
        return ret;
    }

    return 0;
}

