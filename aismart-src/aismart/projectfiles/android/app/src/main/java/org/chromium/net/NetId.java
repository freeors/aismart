package org.chromium.net;

import android.support.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({
        NetId.INVALID
})
@Retention(RetentionPolicy.SOURCE)
public @interface NetId {
    /**
     * Cannot use |kInvalidNetworkHandle| here as the Java generator fails, instead enforce their
     * equality with CHECK in NetworkChangeNotifierAndroid().
     */
    int INVALID = -1;
}