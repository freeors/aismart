package org.chromium.base.metrics;

import android.support.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({
        JSONVerbosityLevel.JSON_VERBOSITY_LEVEL_FULL,
        JSONVerbosityLevel.JSON_VERBOSITY_LEVEL_OMIT_BUCKETS
})
@Retention(RetentionPolicy.SOURCE)
public @interface JSONVerbosityLevel {
    /**
     * The histogram is completely serialized.
     */
    int JSON_VERBOSITY_LEVEL_FULL = 0;
    /**
     * The bucket information is not serialized.
     */
    int JSON_VERBOSITY_LEVEL_OMIT_BUCKETS = 1;
}