package org.libsdl.app;


import android.app.IntentService;
import android.app.Notification;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.BitmapFactory;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.os.Binder;
import android.os.IBinder;
import android.os.Process;
import android.util.Log;

import com.leagor.aismart.R;

import org.webrtc.ThreadUtils;

import static android.app.PendingIntent.getActivity;
import static junit.framework.Assert.assertTrue;

public class SDLService extends IntentService {
    private static final String TAG = "SDL";
    // Audio
    protected AudioTrack mAudioTrack;

    private AudioRecord mAudioRecord;
    private AudioRecordThread audioThread = null;
    private static final long AUDIO_RECORD_THREAD_JOIN_TIMEOUT_MS = 2000;

    protected int lengthPlayout;
    protected byte[] byteBufferPlayout;
    protected short[] shortBufferPlayout;

    protected int lengthCapture;
    protected short[] shortBufferCapture;

    public static boolean mExitThread = true;
    public static boolean mDestroyed;

    public static boolean mPlayoutRequireCreate = false;
    private static int mPlayoutSampleRate = 0;
    private static boolean mPlayoutIs16Bit;
    private static boolean mPlayoutIsStereo;
    private static int mPlayoutDesiredFrames;

    private static int mCaptureSampleRate = 0;
    private static boolean mCaptureIsStereo;
    private static int mCaptureDesiredFrames;

    public static boolean mScreenOn = true;
    public static Notification mNotification;

    private final IBinder mBinder = new LocalBinder();
    /**
     * A constructor is required, and must call the super IntentService(String)
     * constructor with a name for the worker thread.
     */
    public SDLService() {
        super("SDLService");
        Log.i("SDL", "SDLService, SDLService(), " + System.currentTimeMillis());
    }

    public static native boolean nativeFillByteAudioPlayout(byte[] buffer, boolean screenOn);
    public static native boolean nativeFillShortAudioPlayout(short[] buffer, boolean screenOn);

    public static native boolean nativeFillShortAudioCapture(short[] buffer);


    private void registerScreenActionReceiver(){
        final IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_SCREEN_OFF);
        filter.addAction(Intent.ACTION_SCREEN_ON);
        registerReceiver(receiver, filter);
    }

    private final BroadcastReceiver receiver = new BroadcastReceiver(){
        @Override
        public void onReceive(final Context context, final Intent intent) {
            // Do your action here
            if (intent.getAction() == Intent.ACTION_SCREEN_OFF) {
                Log.i("SDL", "SDLService, ACTION_SCREEN_OFF, " + System.currentTimeMillis());
                mScreenOn = false;
/*
                if (SDLActivity.mForegroundService) {
                    startForeground(110, mNotification);
                }
*/
            } else if (intent.getAction() == Intent.ACTION_SCREEN_ON) {
                Log.i("SDL", "SDLService, ACTION_SCREEN_ON, " + System.currentTimeMillis());
                mScreenOn = true;
/*
                if (SDLActivity.mForegroundService) {
                    stopForeground(true);// 停止前台服务--参数：表示是否移除之前的通知
                }
*/
            }
        }

    };

    /**
     * The IntentService calls this method from the default worker thread with
     * the intent that started the service. When this method returns, IntentService
     * stops the service, as appropriate.
     */
    @Override
    protected void onHandleIntent(Intent intent) {
        Log.i("SDL", "SDLService, 1, onHandleIntent(), " + System.currentTimeMillis());

        while (!mExitThread) {
            if (mPlayoutRequireCreate) {
                mPlayoutRequireCreate = false;
                if (mAudioTrack != null) {
                    Log.i("SDL", "SDLService, onHandleIntent, stop audio track!");
                    mAudioTrack.stop();
                    mAudioTrack = null;
                }
                if (audioPlayoutInit(mPlayoutSampleRate, mPlayoutIs16Bit, mPlayoutIsStereo, mPlayoutDesiredFrames) != 0) {
                    Log.i("SDL", "SDLService, onHandleIntent, audioPlayoutInit failed!");
                    break;
                }
            }

            // app will use this thread for execute background task, so call app function every time.
            boolean mPlayoutXmiting = mPlayoutIs16Bit? nativeFillShortAudioPlayout(shortBufferPlayout, mScreenOn): nativeFillByteAudioPlayout(byteBufferPlayout, mScreenOn);
            if (!mPlayoutXmiting) {
                try {
                    Thread.sleep(10);
                } catch(InterruptedException e) {
                    // Nom nom
                }
                continue;
            }
            for (int i = 0; i < lengthPlayout; ) {
                int result = mPlayoutIs16Bit ? mAudioTrack.write(shortBufferPlayout, i, lengthPlayout - i) : mAudioTrack.write(byteBufferPlayout, i, lengthPlayout - i);
                // int result = lengthPlayout;
                if (result > 0) {
                    i += result;
                } else if (result == 0) {
                    try {
                        Thread.sleep(1);
                    } catch (InterruptedException e) {
                            // Nom nom
                    }
                } else {
                    Log.w(TAG, "SDL audio: error return from write(byte)");
                    break;
                }
            }
        }

        if (mAudioTrack != null) {
            mAudioTrack.stop();
            mAudioTrack = null;
        }

        Log.i("SDL", "SDLService, 3, onHandleIntent(), " + System.currentTimeMillis());
    }

    @Override
    public IBinder onBind(Intent intent) {
        // mBound = true;
        Log.i("SDL", "SDLService, onBind(), " + System.currentTimeMillis());
        registerScreenActionReceiver();
        return mBinder;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        // mBound = false;
        // close();
        Log.i("SDL", "SDLService, onUnbind(), " + System.currentTimeMillis());
        return super.onUnbind(intent);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (SDLActivity.mForegroundService) {
            stopForeground(true);// 停止前台服务--参数：表示是否移除之前的通知
        }
        mDestroyed = true;
        Log.i("SDL", "SDLService, onDestroy(), " + System.currentTimeMillis());
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.d(TAG, "onStartCommand()");
        Notification.Builder builder = new Notification.Builder(this.getApplicationContext()); //获取一个Notification构造器
        Intent nfIntent = new Intent(this, SDLActivity.class);

        builder.setContentIntent(PendingIntent.getActivity(this, 0, nfIntent, 0)) // 设置PendingIntent
                .setLargeIcon(BitmapFactory.decodeResource(this.getResources(), R.mipmap.ic_launcher)) // 设置下拉列表中的图标(大图标)
                .setContentTitle(this.getApplicationContext().getString(R.string.app_name)) // 设置下拉列表里的标题
                .setSmallIcon(R.mipmap.ic_launcher) // 设置状态栏内的小图标
                .setContentText(this.getApplicationContext().getString(R.string.foreground_service)) // 设置上下文内容
                .setWhen(System.currentTimeMillis()); // 设置该通知发生的时间

        mNotification = builder.build(); // 获取构建好的Notification
        mNotification.defaults = Notification.DEFAULT_SOUND; //设置为默认的声音

        if (SDLActivity.mForegroundService) {
            startForeground(110, mNotification);
        }
        // stopForeground(true);// 停止前台服务--参数：表示是否移除之前的通知

        return super.onStartCommand(intent, flags, startId);
    }


    /**
     * Local binder class
     */
    public class LocalBinder extends Binder {
        public SDLService getService() {
            return SDLService.this;
        }
    }

    public static void audioSetParam(int sampleRate, boolean is16Bit, boolean isStereo, int desiredFrames) {
        mPlayoutSampleRate = sampleRate;
        mPlayoutIs16Bit = is16Bit;
        mPlayoutIsStereo = isStereo;
        mPlayoutDesiredFrames = desiredFrames;

        mPlayoutRequireCreate = true;
    }

    private int audioPlayoutInit(int sampleRate, boolean is16Bit, boolean isStereo, int desiredFrames) {
        if (mAudioTrack != null) {
            Log.i(TAG, "mAudioTrack must be null before initialization of Audio Track");
            return -1;
        }
        int channelConfig = isStereo ? AudioFormat.CHANNEL_CONFIGURATION_STEREO : AudioFormat.CHANNEL_CONFIGURATION_MONO;
        int audioFormat = is16Bit ? AudioFormat.ENCODING_PCM_16BIT : AudioFormat.ENCODING_PCM_8BIT;
        int frameSize = (isStereo ? 2 : 1) * (is16Bit ? 2 : 1);

        Log.i(TAG, "SDL audio: wanted " + (isStereo ? "stereo" : "mono") + " " + (is16Bit ? "16-bit" : "8-bit") + " " + (sampleRate / 1000f) + "kHz, " + desiredFrames + " frames buffer");

        // Let the user pick a larger buffer if they really want -- but ye
        // gods they probably shouldn't, the minimums are horrifyingly high
        // latency already
        desiredFrames = Math.max(desiredFrames, (AudioTrack.getMinBufferSize(sampleRate, channelConfig, audioFormat) + frameSize - 1) / frameSize);

        mAudioTrack = new AudioTrack(AudioManager.STREAM_MUSIC, sampleRate,
                    channelConfig, audioFormat, desiredFrames * frameSize, AudioTrack.MODE_STREAM);

        // Instantiating AudioTrack can "succeed" without an exception and the track may still be invalid
        // Ref: https://android.googlesource.com/platform/frameworks/base/+/refs/heads/master/media/java/android/media/AudioTrack.java
        // Ref: http://developer.android.com/reference/android/media/AudioTrack.html#getState()

        if (mAudioTrack.getState() != AudioTrack.STATE_INITIALIZED) {
            Log.e(TAG, "Failed during initialization of Audio Track");
            mAudioTrack = null;
            return -1;
        }

        mAudioTrack.play();

        int bufferSizeInBytes = desiredFrames * frameSize;
        lengthPlayout = mPlayoutIs16Bit? bufferSizeInBytes / 2: bufferSizeInBytes;
        byteBufferPlayout = mPlayoutIs16Bit? null: new byte[lengthPlayout];
        shortBufferPlayout = mPlayoutIs16Bit? new short[lengthPlayout]: null;

        Log.i(TAG, "SDL audio: got " + ((mAudioTrack.getChannelCount() >= 2) ? "stereo" : "mono") + " " + ((mAudioTrack.getAudioFormat() == AudioFormat.ENCODING_PCM_16BIT) ? "16-bit" : "8-bit") + " " + (mAudioTrack.getSampleRate() / 1000f) + "kHz, " + desiredFrames + " frames buffer");

        return 0;
    }

    /**
     * This method is called by SDL using JNI.
     */
    public void audioQuit() {
        if (mAudioTrack == null) {
            Log.i(TAG, "SDLService.audioQuit, mAudioTrack == null, do nothing");
            return;
        }
        Log.i(TAG, "SDLService.audioQuit, set mExitThread = true.");
        mExitThread = true;

        while (mAudioTrack != null) {
            try {
                Thread.sleep(1);
            } catch (InterruptedException e) {
                // Nom nom
            }
        }
        Log.i(TAG, "SDLService.audioQuit, exited");
    }

    public boolean isAudioCapturing() {
        return mAudioRecord != null;
    }

    public int audioCaptureInit(int sampleRate, boolean is16Bit, boolean isStereo, int desiredFrames) {
        mCaptureSampleRate = sampleRate;
        mCaptureIsStereo = isStereo;
        mCaptureDesiredFrames = desiredFrames;

        int channelConfig = isStereo ? AudioFormat.CHANNEL_CONFIGURATION_STEREO : AudioFormat.CHANNEL_CONFIGURATION_MONO;
        int audioFormat = is16Bit ? AudioFormat.ENCODING_PCM_16BIT : AudioFormat.ENCODING_PCM_8BIT;
        int frameSize = (isStereo ? 2 : 1) * (is16Bit ? 2 : 1);

        Log.v(TAG, "SDL capture: wanted " + (isStereo ? "stereo" : "mono") + " " + (is16Bit ? "16-bit" : "8-bit") + " " + (sampleRate / 1000f) + "kHz, " + desiredFrames + " frames buffer");
        assertTrue(mAudioRecord == null);

        // Let the user pick a larger buffer if they really want -- but ye
        // gods they probably shouldn't, the minimums are horrifyingly high
        // latency already
        desiredFrames = Math.max(desiredFrames, (AudioRecord.getMinBufferSize(sampleRate, channelConfig, audioFormat) + frameSize - 1) / frameSize);

        mAudioRecord = new AudioRecord(MediaRecorder.AudioSource.DEFAULT, sampleRate,
                    channelConfig, audioFormat, desiredFrames * frameSize);

        // see notes about AudioTrack state in audioOpen(), above. Probably also applies here.
        if (mAudioRecord.getState() != AudioRecord.STATE_INITIALIZED) {
            Log.e(TAG, "Failed during initialization of AudioRecord");
            mAudioRecord.release();
            mAudioRecord = null;
            return -1;
        }

        mAudioRecord.startRecording();

        int bufferSizeInBytes = desiredFrames * frameSize;
        lengthCapture = bufferSizeInBytes / 2;
        shortBufferCapture = new short[lengthCapture];

        Log.v(TAG, "audioCaptureInit, desiredFrames: " + desiredFrames + ", frameSize " + frameSize + ", lengthCapture: " + lengthCapture);

        assertTrue(audioThread == null);
        audioThread = new AudioRecordThread("AudioRecordJavaThread");
        audioThread.start();

        Log.v(TAG, "SDL capture: got " + ((mAudioRecord.getChannelCount() >= 2) ? "stereo" : "mono") + " " + ((mAudioRecord.getAudioFormat() == AudioFormat.ENCODING_PCM_16BIT) ? "16-bit" : "8-bit") + " " + (mAudioRecord.getSampleRate() / 1000f) + "kHz, " + desiredFrames + " frames buffer");

        return 0;
    }

    private void audioCaptureDestroy() {
        if (mAudioRecord != null) {
            Log.i(TAG, "audioCaptureDestroy, will release and null mAudioRecord!");
            if (mAudioRecord.getRecordingState() == AudioRecord.RECORDSTATE_RECORDING) {
                mAudioRecord.stop();
            };
            mAudioRecord.release();
            mAudioRecord = null;
        }
    }

    // is it called by other thread, so requrie syncnize width service
    public void audioCaptureDispose() {
        Log.i(TAG, "SDLService.audioCaptureDispose, process......!");
        assertTrue(audioThread != null);
        audioThread.stopThread();
        if (!ThreadUtils.joinUninterruptibly(audioThread, AUDIO_RECORD_THREAD_JOIN_TIMEOUT_MS)) {
            Log.e(TAG, "Join of AudioRecordJavaThread timed out");
        }
/*
        while (mAudioRecord != null) {
            try {
                Thread.sleep(1);
            } catch (InterruptedException e) {
                // Nom nom
            }
        }
*/
        audioThread = null;
        audioCaptureDestroy();
        Log.i(TAG, "SDLService.audioCaptureDispose, impletemented!");
    }

    /**
     * Audio thread which keeps calling ByteBuffer.read() waiting for audio
     * to be recorded. Feeds recorded data to the native counterpart as a
     * periodic sequence of callbacks using DataIsRecorded().
     * This thread uses a Process.THREAD_PRIORITY_URGENT_AUDIO priority.
     */
    private class AudioRecordThread extends Thread {
        private volatile boolean keepAlive = true;

        public AudioRecordThread(String name) {
            super(name);
        }

        @Override
        public void run() {
            Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO);
            Log.d(TAG, "AudioRecordThread" + SDLActivity.getThreadInfo());
            assertTrue(mAudioRecord.getRecordingState() == AudioRecord.RECORDSTATE_RECORDING);

            while (keepAlive) {
                for (int i = 0; i < lengthCapture; ) {
                    int result = mAudioRecord.read(shortBufferCapture, i, lengthCapture - i);
                    if (result > 0) {
                        i += result;
                    } else if (result == 0) {
                        try {
                            Thread.sleep(1);
                        } catch (InterruptedException e) {
                            // Nom nom
                        }
                    } else {
                        Log.e(TAG, "AudioRecord.read failed: " + result);
                        keepAlive = false;
                        break;
                    }
                }
                nativeFillShortAudioCapture(shortBufferCapture);
            }

            try {
                if (mAudioRecord != null) {
                    mAudioRecord.stop();
                }
            } catch (IllegalStateException e) {
                Log.e(TAG, "AudioRecord.stop failed: " + e.getMessage());
            }
            Log.d(TAG, "AudioRecordThread" + SDLActivity.getThreadInfo() + " exit");
        }

        // Stops the inner thread loop and also calls AudioRecord.stop().
        // Does not block the calling thread.
        public void stopThread() {
            Log.d(TAG, "stopThread");
            keepAlive = false;
        }
    }
}