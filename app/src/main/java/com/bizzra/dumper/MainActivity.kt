package com.bizzra.dumper

import android.app.Activity
import android.content.Intent
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.ColorDrawable
import android.graphics.drawable.GradientDrawable
import android.graphics.drawable.LayerDrawable
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.TextView

class MainActivity : Activity() {

    private val bgLight = Color.parseColor("#FFF4E0") // Cream/off-white background
    private val yellowAccent = Color.parseColor("#FFD93D") // Neobrutalist yellow
    private val pinkAccent = Color.parseColor("#FF6B6B") // Neobrutalist pink/coral
    private val blackColor = Color.parseColor("#000000") // Solid black for borders
    private val whiteColor = Color.parseColor("#FFFFFF") // Solid white

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(bgLight)
            layoutParams = ViewGroup.LayoutParams(-1, -1)
        }

        val content = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
            setPadding(dp(28), dp(64), dp(28), dp(32))
            layoutParams = LinearLayout.LayoutParams(-1, -1)
        }

        // ─── Logo / Icon area (Neobrutalist Box) ───
        val iconBox = TextView(this).apply {
            text = "UE"
            textSize = 38f
            setTextColor(blackColor)
            typeface = Typeface.create("sans-serif-black", Typeface.BOLD)
            gravity = Gravity.CENTER
            val size = dp(80)
            layoutParams = LinearLayout.LayoutParams(size, size).apply {
                gravity = Gravity.CENTER_HORIZONTAL
                bottomMargin = dp(24)
            }
            background = createNeobrutalistBg(yellowAccent)
        }
        content.addView(iconBox)

        // ─── Title ───
        val title = TextView(this).apply {
            text = "UE DUMPER"
            textSize = 28f
            typeface = Typeface.create("sans-serif-black", Typeface.NORMAL)
            setTextColor(blackColor)
            gravity = Gravity.CENTER
            setPadding(0, 0, 0, dp(8))
        }
        content.addView(title)

        // ─── Version badge ───
        val versionBadge = TextView(this).apply {
            text = "v2.0 DUAL MODE"
            textSize = 12f
            typeface = Typeface.create("sans-serif-black", Typeface.NORMAL)
            setTextColor(blackColor)
            gravity = Gravity.CENTER
            setPadding(dp(12), dp(6), dp(12), dp(6))
            background = createNeobrutalistBg(whiteColor)
            layoutParams = LinearLayout.LayoutParams(-2, -2).apply {
                gravity = Gravity.CENTER_HORIZONTAL
                bottomMargin = dp(32)
            }
        }
        content.addView(versionBadge)

        // ─── Injection Mode Card ───
        content.addView(
            createModeCard(
                icon = "💉",
                title = "INJECTION MODE",
                desc = "Inject into a target APK.\nDirect memory access. No root.",
                accentColor = yellowAccent
            ) {
                DumperCore.SetMode(DumperCore.MODE_INJECTED)
                DumperCore.CheckOverlayPermission(this)
            }
        )

        content.addView(spacer(dp(20)))

        // ─── Standalone Mode Card ───
        content.addView(
            createModeCard(
                icon = "💀",
                title = "STANDALONE MODE",
                desc = "Read via process_vm_readv.\nCross-process memory. Root required.",
                accentColor = pinkAccent
            ) {
                DumperCore.SetMode(DumperCore.MODE_STANDALONE)
                startActivity(Intent(this, ProcessListActivity::class.java))
            }
        )

        content.addView(spacer(dp(20)))

        // ─── View Logs Card ───
        val blueAccent = Color.parseColor("#4D96FF")
        content.addView(
            createModeCard(
                icon = "📋",
                title = "VIEW LOGS",
                desc = "In-app Logcat replacement.\nReal-time debug output.",
                accentColor = blueAccent
            ) {
                startActivity(Intent(this, LogViewerActivity::class.java))
            }
        )

        // ─── Spacer to push footer down ───
        content.addView(View(this).apply {
            layoutParams = LinearLayout.LayoutParams(-1, 0, 1f)
        })

        // ─── Divider ───
        content.addView(View(this).apply {
            setBackgroundColor(blackColor)
            layoutParams = LinearLayout.LayoutParams(-1, dp(4)).apply {
                topMargin = dp(16)
                bottomMargin = dp(16)
            }
        })

        // ─── Footer ───
        val footer = TextView(this).apply {
            text = "MADE BY ASCARRE • EDUCATIONAL USE ONLY"
            textSize = 10f
            typeface = Typeface.create("sans-serif-black", Typeface.NORMAL)
            setTextColor(blackColor)
            gravity = Gravity.CENTER
        }
        content.addView(footer)

        root.addView(content)
        setContentView(root)
    }

    private fun createModeCard(
        icon: String,
        title: String,
        desc: String,
        accentColor: Int,
        onClick: () -> Unit
    ): View {
        val cardContainer = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(20), dp(20), dp(20), dp(20))
            layoutParams = LinearLayout.LayoutParams(-1, -2)
            background = createNeobrutalistBg(accentColor)
            isClickable = true
            isFocusable = true
            
            // Interaction effect
            setOnClickListener {
                // Flash white on click
                background = createNeobrutalistBg(whiteColor)
                postDelayed({
                    background = createNeobrutalistBg(accentColor)
                }, 100)
                onClick()
            }
        }

        // Icon box inside card
        val iconView = TextView(this).apply {
            text = icon
            textSize = 24f
            gravity = Gravity.CENTER
            val size = dp(50)
            layoutParams = LinearLayout.LayoutParams(size, size).apply {
                rightMargin = dp(16)
            }
            background = createNeobrutalistBg(whiteColor)
        }
        cardContainer.addView(iconView)

        // Text column
        val textCol = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(0, -2, 1f)
        }

        val titleView = TextView(this).apply {
            text = title
            textSize = 18f
            setTextColor(blackColor)
            typeface = Typeface.create("sans-serif-black", Typeface.NORMAL)
            setPadding(0, 0, 0, dp(4))
        }
        textCol.addView(titleView)

        val descView = TextView(this).apply {
            text = desc
            textSize = 13f
            setTextColor(blackColor)
            typeface = Typeface.create("sans-serif-medium", Typeface.NORMAL)
            setLineSpacing(dp(2).toFloat(), 1f)
        }
        textCol.addView(descView)

        cardContainer.addView(textCol)
        return cardContainer
    }

    // Helper to create a thick-bordered, hard-shadow box typical of neobrutalism
    private fun createNeobrutalistBg(fillColor: Int): LayerDrawable {
        // Shadow layer (solid black, offset right and down)
        val shadow = GradientDrawable().apply {
            setColor(blackColor)
            setStroke(dp(3), blackColor)
        }
        // Top layer
        val top = GradientDrawable().apply {
            setColor(fillColor)
            setStroke(dp(3), blackColor)
        }

        val layers = arrayOf(shadow, top)
        val layerDrawable = LayerDrawable(layers)
        // Offset the shadow by 5dp right and down
        layerDrawable.setLayerInset(0, dp(5), dp(5), 0, 0)
        layerDrawable.setLayerInset(1, 0, 0, dp(5), dp(5))
        return layerDrawable
    }

    private fun spacer(height: Int): View {
        return View(this).apply {
            layoutParams = LinearLayout.LayoutParams(-1, height)
        }
    }

    private fun dp(v: Int): Int = (v * resources.displayMetrics.density).toInt()
}