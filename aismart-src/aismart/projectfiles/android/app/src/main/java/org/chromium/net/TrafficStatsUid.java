package org.chromium.net;

import android.support.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({
        TrafficStatsUid.UNSET
})
@Retention(RetentionPolicy.SOURCE)
public @interface TrafficStatsUid {
    int UNSET = -1;
}