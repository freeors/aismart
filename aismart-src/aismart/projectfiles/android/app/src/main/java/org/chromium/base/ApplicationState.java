package org.chromium.base;

import android.support.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({
        ApplicationState.UNKNOWN, ApplicationState.HAS_RUNNING_ACTIVITIES,
        ApplicationState.HAS_PAUSED_ACTIVITIES, ApplicationState.HAS_STOPPED_ACTIVITIES,
        ApplicationState.HAS_DESTROYED_ACTIVITIES
})
@Retention(RetentionPolicy.SOURCE)
public @interface ApplicationState {
    int UNKNOWN = 0;
    int HAS_RUNNING_ACTIVITIES = 1;
    int HAS_PAUSED_ACTIVITIES = 2;
    int HAS_STOPPED_ACTIVITIES = 3;
    int HAS_DESTROYED_ACTIVITIES = 4;
}