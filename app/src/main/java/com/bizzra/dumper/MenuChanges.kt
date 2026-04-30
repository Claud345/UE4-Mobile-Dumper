package com.bizzra.dumper

import android.content.Context
import android.content.Intent

class MenuChanges(var ctx: Context?) {

    external fun Changes(context: Context?, fInt: Int, fName: String?, value: Int, bool: Boolean, string: String?)

    fun changeFeatureInt(featureName: String?, featureNum: Int, value: Int) {
        // Intercept "View Logs" button (feature 14) — open LogViewerActivity
        if (featureNum == 14) {
            ctx?.let {
                val intent = Intent(it, LogViewerActivity::class.java)
                intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK
                it.startActivity(intent)
            }
            return
        }
        Changes(ctx, featureNum, featureName, value, false, null)
    }

    fun changeFeatureString(featureName: String?, featureNum: Int, str: String?) {
        Changes(ctx, featureNum, featureName, 0, false, str)
    }

    fun changeFeatureBool(featureName: String?, featureNum: Int, bool: Boolean) {
        Changes(ctx, featureNum, featureName, 0, bool, null)
    }
}