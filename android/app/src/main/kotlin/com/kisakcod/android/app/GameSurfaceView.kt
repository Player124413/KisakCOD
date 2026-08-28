package com.kisakcod.android.app

import android.content.Context
import android.view.SurfaceHolder
import android.view.SurfaceView

/**
 * Holds the native EGL surface. The overlay (TouchOverlay) sits on top of this
 * view in the same frame; native renders beneath it.
 */
class GameSurfaceView(context: Context) : SurfaceView(context), SurfaceHolder.Callback {

    init {
        holder.addCallback(this)
        isFocusable = true
        isFocusableInTouchMode = true
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        JniBridge.nativeSurfaceCreated(holder.surface)
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        JniBridge.nativeSurfaceChanged(width, height)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        JniBridge.nativeSurfaceDestroyed()
    }
}