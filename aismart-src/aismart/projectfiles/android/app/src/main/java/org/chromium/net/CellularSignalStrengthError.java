package org.chromium.net;

import android.support.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({
        CellularSignalStrengthError.ERROR_NOT_SUPPORTED
})
@Retention(RetentionPolicy.SOURCE)
public @interface CellularSignalStrengthError {
    /**
     * Value returned by CellularSignalStrength APIs when a valid value is unavailable. This value is
     * same as INT32_MIN, but the following code uses the explicit value of INT32_MIN so that the
     * auto-generated Java enums work correctly.
     */
    int ERROR_NOT_SUPPORTED = -2147483648;
}