package org.chromium.net;

import android.support.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({
        TrafficStatsError.ERROR_NOT_SUPPORTED
})
@Retention(RetentionPolicy.SOURCE)
public @interface TrafficStatsError {
    /**
     * Value returned by AndroidTrafficStats APIs when a valid value is unavailable.
     */
    int ERROR_NOT_SUPPORTED = 0;
}