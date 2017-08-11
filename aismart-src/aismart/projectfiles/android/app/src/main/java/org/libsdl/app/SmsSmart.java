/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.libsdl.app;

import android.content.Context;

import android.os.Build;
import android.telephony.SmsManager;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.util.ArrayMap;
import android.util.Log;

import org.webrtc.ContextUtils;

import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

/**
 * Peer connection client implementation.
 *
 * <p>All public methods are routed to local looper thread.
 * All PeerConnectionEvents callbacks are invoked from the same looper thread.
 * This class is a singleton.
 */

public class SmsSmart {
    private static final String TAG = "SmsSmart";

    private Context context;
    private static final Map<Integer, SmsManager> mSubIdSmsManager = new ArrayMap<Integer, SmsManager>();

    SmsSmart(long nativeVideoCapturer) {
        this.context = ContextUtils.getApplicationContext();
    }

    public class SimInfo {
        public boolean mLevel22 = false;
        public int mSlot = 0;
        public String mCarrier = "Unknown";
        public String mNumber = "Unknown";

        boolean getLevel22() { return mLevel22; }
        int getSlot() { return mSlot; }
        String getCarrier() { return mCarrier; }
        String getNumber() { return mNumber; }
    }
    private List<SimInfo> getSimInfo() {
        final SubscriptionManager sManager = (SubscriptionManager) context.getSystemService(Context.TELEPHONY_SUBSCRIPTION_SERVICE);

        List<SimInfo> result = new ArrayList<SimInfo>();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP_MR1) {
            List<SubscriptionInfo> subs = sManager.getActiveSubscriptionInfoList();
            // if no ActiveSubscriptionInfo, subs is null.
            if (subs != null) {
                for (SubscriptionInfo sub : subs) {
                    SimInfo info = new SimInfo();
                    info.mLevel22 = true;
                    info.mSlot = sub.getSimSlotIndex();
                    String carrier = sub.getCarrierName().toString();
                    info.mCarrier = carrier;
                    info.mNumber = sub.getNumber();

                    result.add(info);
                }
            }
        } else {
            SimInfo info = new SimInfo();
            info.mLevel22 = false;
            info.mCarrier = "<carrier>";
            info.mNumber = "<number>";
            result.add(info);
        }
        return result;
    }

    private void createSmsManager() {
        final SubscriptionManager sManager = (SubscriptionManager) context.getSystemService(Context.TELEPHONY_SUBSCRIPTION_SERVICE);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP_MR1) {
            List<SubscriptionInfo> subs = sManager.getActiveSubscriptionInfoList();
            for (SubscriptionInfo sub : subs) {
                int subId = sub.getSubscriptionId();

                SmsManager manager = null;
                try {
                    manager = SmsManager.getSmsManagerForSubscriptionId(subId);
                } catch (Exception e) {
                    e.printStackTrace();
                }

                if (manager == null) {
                    manager = SmsManager.getDefault();
                }
                mSubIdSmsManager.put(sub.getSimSlotIndex(), manager);
            }
        } else {
            mSubIdSmsManager.put(0, SmsManager.getDefault());
        }
    }

    private boolean Send(int slot, String targetPhone, String message) {
        if (slot < 0 || slot > 1) {
            return false;
        }
        if (mSubIdSmsManager.isEmpty()) {
            createSmsManager();
        }
        SmsManager smsManager = mSubIdSmsManager.get(slot);
        if (smsManager == null) {
            return false;
        }
        ArrayList<String> smses = smsManager.divideMessage(message);
        Iterator<String> iterator = smses.iterator();
        while (iterator.hasNext()) {
            String temp = iterator.next();
            smsManager.sendTextMessage(targetPhone, null, temp, null, null);
        }
        return true;
    }
}
