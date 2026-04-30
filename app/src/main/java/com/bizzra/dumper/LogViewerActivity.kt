package com.bizzra.dumper

import android.app.Activity
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.graphics.drawable.LayerDrawable
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.text.Spannable
import android.text.SpannableStringBuilder
import android.text.style.ForegroundColorSpan
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.*

/**
 * LogViewerActivity — In-app Logcat replacement
 * Displays color-coded, filterable, auto-scrolling logs from the native dumper engine.
 * Designed for CI/GitHub Actions builds where Android Studio is unavailable.
 */
class LogViewerActivity : Activity() {

    // ─── Neobrutalist Color Palette ───
    private val bgLight      = Color.parseColor("#1A1A2E")  // Dark navy background
    private val bgCard       = Color.parseColor("#16213E")  // Slightly lighter card bg
    private val bgTerminal   = Color.parseColor("#0F0F1A")  // Terminal-dark background
    private val yellowAccent = Color.parseColor("#FFD93D")
    private val pinkAccent   = Color.parseColor("#FF6B6B")
    private val greenAccent  = Color.parseColor("#6BCB77")
    private val blueAccent   = Color.parseColor("#4D96FF")
    private val orangeAccent = Color.parseColor("#FF8C32")
    private val whiteColor   = Color.parseColor("#E8E8E8")
    private val grayColor    = Color.parseColor("#888888")
    private val blackColor   = Color.parseColor("#000000")

    // ─── Log Level Colors ───
    private val colorVerbose = Color.parseColor("#888888")
    private val colorDebug   = Color.parseColor("#4D96FF")
    private val colorInfo    = Color.parseColor("#6BCB77")
    private val colorWarn    = Color.parseColor("#FFD93D")
    private val colorError   = Color.parseColor("#FF6B6B")
    private val colorFatal   = Color.parseColor("#FF2222")

    // ─── State ───
    private var logTextView: TextView? = null
    private var scrollView: ScrollView? = null
    private var statusText: TextView? = null
    private var filterSpinner: Spinner? = null
    private var autoScroll = true
    private var currentFilter = 0 // 0=V, 1=D, 2=I, 3=W, 4=E
    private var pollHandler: Handler? = null
    private var isPaused = false

    private val pollRunnable = object : Runnable {
        override fun run() {
            if (!isPaused) {
                refreshLogs()
            }
            pollHandler?.postDelayed(this, 500) // Poll every 500ms
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(bgLight)
            layoutParams = ViewGroup.LayoutParams(-1, -1)
        }

        // ═══ Header Bar ═══
        val header = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(16), dp(12), dp(16), dp(12))
            setBackgroundColor(bgCard)
            layoutParams = LinearLayout.LayoutParams(-1, -2)
        }

        // Back button
        val backBtn = TextView(this).apply {
            text = "◀"
            textSize = 20f
            setTextColor(whiteColor)
            setPadding(dp(8), dp(4), dp(16), dp(4))
            setOnClickListener { finish() }
        }
        header.addView(backBtn)

        // Title
        val titleView = TextView(this).apply {
            text = "LOG VIEWER"
            textSize = 18f
            typeface = Typeface.create("sans-serif-black", Typeface.NORMAL)
            setTextColor(yellowAccent)
            layoutParams = LinearLayout.LayoutParams(0, -2, 1f)
        }
        header.addView(titleView)

        // Live indicator
        statusText = TextView(this).apply {
            text = "● LIVE"
            textSize = 12f
            typeface = Typeface.create("sans-serif-black", Typeface.NORMAL)
            setTextColor(greenAccent)
            setPadding(dp(8), dp(4), dp(8), dp(4))
        }
        header.addView(statusText)

        root.addView(header)

        // ═══ Toolbar ═══
        val toolbar = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(12), dp(8), dp(12), dp(8))
            setBackgroundColor(Color.parseColor("#111133"))
            layoutParams = LinearLayout.LayoutParams(-1, -2)
        }

        // Filter label
        toolbar.addView(TextView(this).apply {
            text = "FILTER:"
            textSize = 11f
            typeface = Typeface.create("sans-serif-black", Typeface.NORMAL)
            setTextColor(grayColor)
            setPadding(0, 0, dp(8), 0)
        })

        // Filter level buttons
        val filterLevels = arrayOf("V", "D", "I", "W", "E")
        val filterColors = intArrayOf(colorVerbose, colorDebug, colorInfo, colorWarn, colorError)

        for (i in filterLevels.indices) {
            val btn = TextView(this).apply {
                text = filterLevels[i]
                textSize = 12f
                typeface = Typeface.create("sans-serif-black", Typeface.BOLD)
                setTextColor(if (i == currentFilter) blackColor else filterColors[i])
                gravity = Gravity.CENTER
                setPadding(dp(10), dp(6), dp(10), dp(6))
                val bg = GradientDrawable().apply {
                    setColor(if (i == currentFilter) filterColors[i] else Color.TRANSPARENT)
                    setStroke(dp(2), filterColors[i])
                    cornerRadius = dp(4).toFloat()
                }
                background = bg
                layoutParams = LinearLayout.LayoutParams(-2, -2).apply {
                    setMargins(dp(3), 0, dp(3), 0)
                }

                val filterIdx = i
                setOnClickListener {
                    currentFilter = filterIdx
                    // Update all filter button appearances
                    val parent = it.parent as LinearLayout
                    for (ci in 0 until parent.childCount) {
                        val child = parent.getChildAt(ci)
                        if (child is TextView && child.text.length == 1 && "VDIWE".contains(child.text)) {
                            val idx = "VDIWE".indexOf(child.text)
                            if (idx >= 0) {
                                val isActive = idx == currentFilter
                                child.setTextColor(if (isActive) blackColor else filterColors[idx])
                                val cbg = GradientDrawable().apply {
                                    setColor(if (isActive) filterColors[idx] else Color.TRANSPARENT)
                                    setStroke(dp(2), filterColors[idx])
                                    cornerRadius = dp(4).toFloat()
                                }
                                child.background = cbg
                            }
                        }
                    }
                    forceFullRefresh()
                }
            }
            toolbar.addView(btn)
        }

        // Spacer
        toolbar.addView(View(this).apply {
            layoutParams = LinearLayout.LayoutParams(0, 1, 1f)
        })

        // Pause/Resume button
        val pauseBtn = TextView(this).apply {
            text = "⏸ PAUSE"
            textSize = 11f
            typeface = Typeface.create("sans-serif-black", Typeface.NORMAL)
            setTextColor(whiteColor)
            gravity = Gravity.CENTER
            setPadding(dp(10), dp(6), dp(10), dp(6))
            background = GradientDrawable().apply {
                setColor(Color.parseColor("#333355"))
                setStroke(dp(2), grayColor)
                cornerRadius = dp(4).toFloat()
            }
            layoutParams = LinearLayout.LayoutParams(-2, -2).apply {
                setMargins(dp(4), 0, dp(4), 0)
            }
            setOnClickListener {
                isPaused = !isPaused
                if (isPaused) {
                    this.text = "▶ RESUME"
                    this.setTextColor(yellowAccent)
                    statusText?.text = "⏸ PAUSED"
                    statusText?.setTextColor(orangeAccent)
                } else {
                    this.text = "⏸ PAUSE"
                    this.setTextColor(whiteColor)
                    statusText?.text = "● LIVE"
                    statusText?.setTextColor(greenAccent)
                    forceFullRefresh()
                }
            }
        }
        toolbar.addView(pauseBtn)

        // Clear button
        val clearBtn = TextView(this).apply {
            text = "✕ CLEAR"
            textSize = 11f
            typeface = Typeface.create("sans-serif-black", Typeface.NORMAL)
            setTextColor(pinkAccent)
            gravity = Gravity.CENTER
            setPadding(dp(10), dp(6), dp(10), dp(6))
            background = GradientDrawable().apply {
                setColor(Color.TRANSPARENT)
                setStroke(dp(2), pinkAccent)
                cornerRadius = dp(4).toFloat()
            }
            layoutParams = LinearLayout.LayoutParams(-2, -2).apply {
                setMargins(dp(4), 0, 0, 0)
            }
            setOnClickListener {
                DumperCore.ClearLogs()
                logTextView?.text = ""
            }
        }
        toolbar.addView(clearBtn)

        root.addView(toolbar)

        // ═══ Divider ═══
        root.addView(View(this).apply {
            setBackgroundColor(yellowAccent)
            layoutParams = LinearLayout.LayoutParams(-1, dp(2))
        })

        // ═══ Log Output Area ═══
        scrollView = ScrollView(this).apply {
            layoutParams = LinearLayout.LayoutParams(-1, 0, 1f)
            setBackgroundColor(bgTerminal)
            isFillViewport = true
        }

        logTextView = TextView(this).apply {
            setTextColor(whiteColor)
            textSize = 11f
            typeface = Typeface.MONOSPACE
            setPadding(dp(12), dp(8), dp(12), dp(8))
            setTextIsSelectable(true)
            layoutParams = ViewGroup.LayoutParams(-1, -2)
        }

        scrollView!!.addView(logTextView)
        root.addView(scrollView)

        // ═══ Footer Bar ═══
        val footer = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(12), dp(8), dp(12), dp(8))
            setBackgroundColor(bgCard)
            layoutParams = LinearLayout.LayoutParams(-1, -2)
        }

        // Log file path
        val filePathText = TextView(this).apply {
            text = "Log: ${DumperCore.GetLogFilePath()}"
            textSize = 10f
            typeface = Typeface.MONOSPACE
            setTextColor(grayColor)
            setSingleLine(true)
            layoutParams = LinearLayout.LayoutParams(0, -2, 1f)
        }
        footer.addView(filePathText)

        // Auto-scroll toggle
        val scrollToggle = TextView(this).apply {
            text = "⬇ AUTO"
            textSize = 11f
            typeface = Typeface.create("sans-serif-black", Typeface.NORMAL)
            setTextColor(if (autoScroll) greenAccent else grayColor)
            setPadding(dp(8), dp(4), dp(8), dp(4))
            setOnClickListener {
                autoScroll = !autoScroll
                this.setTextColor(if (autoScroll) greenAccent else grayColor)
                if (autoScroll) {
                    scrollView?.post { scrollView?.fullScroll(View.FOCUS_DOWN) }
                }
            }
        }
        footer.addView(scrollToggle)

        root.addView(footer)

        setContentView(root)

        // ═══ Start polling ═══
        pollHandler = Handler(Looper.getMainLooper())
        forceFullRefresh()
        pollHandler?.postDelayed(pollRunnable, 500)
    }

    private fun forceFullRefresh() {
        try {
            val rawLog = DumperCore.GetFormattedLog(currentFilter)
            if (rawLog.isNotEmpty()) {
                logTextView?.text = colorizeLog(rawLog)
            } else {
                logTextView?.text = buildWelcomeMessage()
            }
            if (autoScroll) {
                scrollView?.post { scrollView?.fullScroll(View.FOCUS_DOWN) }
            }
        } catch (e: Exception) {
            logTextView?.text = "Error reading logs: ${e.message}"
        }
    }

    private fun refreshLogs() {
        try {
            val newEntries = DumperCore.PollNewLogs(currentFilter)
            if (newEntries.isNotEmpty()) {
                logTextView?.append(colorizeLog(newEntries))
                if (autoScroll) {
                    scrollView?.post { scrollView?.fullScroll(View.FOCUS_DOWN) }
                }
            }
        } catch (_: Exception) {}
    }

    private fun colorizeLog(raw: String): SpannableStringBuilder {
        val builder = SpannableStringBuilder()
        val lines = raw.split("\n")

        for (line in lines) {
            if (line.isEmpty()) continue
            val parts = line.split("|", limit = 4)
            if (parts.size < 4) {
                builder.append(line)
                builder.append("\n")
                continue
            }

            val level = parts[0]
            val tag = parts[1]
            val time = parts[2]
            val msg = parts[3]

            val color = when (level) {
                "V" -> colorVerbose
                "D" -> colorDebug
                "I" -> colorInfo
                "W" -> colorWarn
                "E" -> colorError
                "F" -> colorFatal
                else -> whiteColor
            }

            // Format: [HH:MM:SS.mmm] L/Tag: message
            val formatted = "$time $level/$tag: $msg\n"
            val start = builder.length
            builder.append(formatted)
            // Color the level indicator
            val levelStart = start + time.length + 1
            val levelEnd = levelStart + level.length
            if (levelEnd <= builder.length) {
                builder.setSpan(ForegroundColorSpan(color), levelStart, levelEnd, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
            }
            // Color the timestamp
            builder.setSpan(ForegroundColorSpan(grayColor), start, start + time.length, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
        }

        return builder
    }

    private fun buildWelcomeMessage(): SpannableStringBuilder {
        val builder = SpannableStringBuilder()
        val welcome = """
            |  ╔══════════════════════════════════════╗
            |  ║       UE DUMPER — LOG VIEWER         ║
            |  ╠══════════════════════════════════════╣
            |  ║  Waiting for log entries...          ║
            |  ║                                      ║
            |  ║  • Logs appear here in real-time     ║
            |  ║  • Use filter buttons above          ║
            |  ║  • Logs also saved to file           ║
            |  ╚══════════════════════════════════════╝
            |
        """.trimMargin()
        builder.append(welcome)
        builder.setSpan(ForegroundColorSpan(yellowAccent), 0, builder.length, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE)
        return builder
    }

    override fun onDestroy() {
        super.onDestroy()
        pollHandler?.removeCallbacks(pollRunnable)
    }

    private fun dp(v: Int): Int = (v * resources.displayMetrics.density).toInt()
}
