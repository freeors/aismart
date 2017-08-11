package org.chromium.base;
public class BuildConfig {
    public static boolean isMultidexEnabled() {
        return false;
    }
    public static String FIREBASE_APP_ID = "";
    public static boolean DCHECK_IS_ON = true;
    public static boolean IS_UBSAN = false;
    public static String[] COMPRESSED_LOCALES = {};
    public static String[] UNCOMPRESSED_LOCALES = {};
}
