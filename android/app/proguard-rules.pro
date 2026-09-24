# KisakCOD proguard rules.
#
# Keep every JNI entry point. These are reached from native code by name, so
# the obfuscator renaming them breaks input delivery with no Java stack trace.
-keep class com.kisak.cod.input.InputBridge { *; }
-keepclasseswithmembernames class * {
    native <methods>;
}

# The touch overlay is inflated from XML and driven by reflection-free code,
# but keep its constructor signature regardless.
-keep class com.kisak.cod.input.TouchControlsView {
    public <init>(android.content.Context);
    public <init>(android.content.Context, android.util.AttributeSet);
}

# Settings model is serialised to JSON with org.json; keep field names stable.
-keepclassmembers class com.kisak.cod.input.** { *; }
-keepclassmembers class com.kisak.cod.settings.** { *; }
