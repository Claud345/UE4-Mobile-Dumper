package com.bizzra.dumper

import android.app.Activity
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.graphics.drawable.LayerDrawable
import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast

class ProcessListActivity : Activity() {

    // ─── Neobrutalist Palette ───
    private val bgLight = Color.parseColor("#FFF4E0") // Cream/off-white background
    private val yellowAccent = Color.parseColor("#FFD93D")
    private val pinkAccent = Color.parseColor("#FF6B6B")
    private val blueAccent = Color.parseColor("#4D96FF")
    private val greenAccent = Color.parseColor("#6BCB77")
    private val blackColor = Color.parseColor("#000000")
    private val whiteColor = Color.parseColor("#FFFFFF")

    private var processListLayout: LinearLayout? = null
    private var allProcesses: List<Pair<Int, String>> = emptyList()
    private var countLabel: TextView? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(bgLight)
            layoutParams = ViewGroup.LayoutParams(-1, -1)
        }

        // ════════════════════ HEADER ════════════════════
        val header = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(24), dp(44), dp(24), dp(12))
            setBackgroundColor(bgLight)
        }

        // Back + title row
        val topRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(0, 0, 0, dp(4))
        }

        val backBtn = TextView(this).apply {
            text = "BACK"
            textSize = 14f
            setTextColor(blackColor)
            typeface = Typeface.create("sans-serif-black", Typeface.NORMAL)
            setPadding(dp(12), dp(8), dp(12), dp(8))
            background = createNeobrutalistBg(yellowAccent)
            setOnClickListener { finish() }
            layoutParams = LinearLayout.LayoutParams(-2, -2).apply { rightMargin = dp(16) }
        }
        topRow.addView(backBtn)

        val headerTitle = TextView(this).apply {
            text = "SELECT PROCESS"
            textSize = 22f
            setTextColor(blackColor)
            typeface = Typeface.create("sans-serif-black", Typeface.NORMAL)
            layoutParams = LinearLayout.LayoutParams(0, -2, 1f)
        }
        topRow.addView(headerTitle)

        header.addView(topRow)

        // Subtitle
        val headerSub = TextView(this).apply {
            text = "CHOOSE A RUNNING GAME PROCESS TO ATTACH THE DUMPER"
            textSize = 12f
            setTextColor(blackColor)
            typeface = Typeface.create("sans-serif-medium", Typeface.NORMAL)
            setPadding(0, dp(8), 0, dp(12))
        }
        header.addView(headerSub)

        // Root status badge
        val hasRoot = try { DumperCore.HasRootAccess() } catch (_: Exception) { false }
        val rootBadge = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(12), dp(10), dp(12), dp(10))
            background = createNeobrutalistBg(if (hasRoot) greenAccent else pinkAccent)
            layoutParams = LinearLayout.LayoutParams(-1, -2).apply { bottomMargin = dp(16) }
        }

        val rootText = TextView(this).apply {
            text = if (hasRoot) "ROOT DETECTED: READY TO ATTACH" else "ROOT NOT DETECTED: REQUIRES ROOT"
            textSize = 13f
            typeface = Typeface.create("sans-serif-black", Typeface.NORMAL)
            setTextColor(blackColor)
        }
        rootBadge.addView(rootText)
        header.addView(rootBadge)

        // ─── Search bar ───
        val searchBar = EditText(this).apply {
            hint = "FILTER BY PACKAGE NAME..."
            setHintTextColor(Color.parseColor("#777777"))
            setTextColor(blackColor)
            textSize = 14f
            typeface = Typeface.create("sans-serif-black", Typeface.NORMAL)
            isSingleLine = true
            setPadding(dp(16), dp(16), dp(16), dp(16))
            background = createNeobrutalistBg(whiteColor)
            layoutParams = LinearLayout.LayoutParams(-1, -2).apply { bottomMargin = dp(12) }
            addTextChangedListener(object : TextWatcher {
                override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
                override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
                override fun afterTextChanged(s: Editable?) {
                    filterProcesses(s?.toString() ?: "")
                }
            })
            setOnFocusChangeListener { _, hasFocus ->
                background = createNeobrutalistBg(if (hasFocus) yellowAccent else whiteColor)
            }
        }
        header.addView(searchBar)

        // ─── Manual attach row ───
        val manualRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(0, dp(4), 0, dp(8))
        }

        val manualInput = EditText(this).apply {
            hint = "COM.EXAMPLE.GAME"
            setHintTextColor(Color.parseColor("#777777"))
            setTextColor(blackColor)
            textSize = 14f
            typeface = Typeface.create("sans-serif-black", Typeface.NORMAL)
            isSingleLine = true
            setPadding(dp(16), dp(16), dp(16), dp(16))
            background = createNeobrutalistBg(whiteColor)
            layoutParams = LinearLayout.LayoutParams(0, -2, 1f)
            setOnFocusChangeListener { _, hasFocus ->
                background = createNeobrutalistBg(if (hasFocus) blueAccent else whiteColor)
            }
        }
        manualRow.addView(manualInput)

        val attachBtn = TextView(this).apply {
            text = "ATTACH"
            textSize = 14f
            setTextColor(blackColor)
            typeface = Typeface.create("sans-serif-black", Typeface.NORMAL)
            gravity = Gravity.CENTER
            setPadding(dp(20), dp(16), dp(20), dp(16))
            background = createNeobrutalistBg(blueAccent)
            layoutParams = LinearLayout.LayoutParams(-2, -2).apply { leftMargin = dp(12) }
            setOnClickListener {
                background = createNeobrutalistBg(whiteColor)
                postDelayed({ background = createNeobrutalistBg(blueAccent) }, 100)
                val pkg = manualInput.text.toString().trim()
                if (pkg.isNotEmpty()) {
                    attachToPackage(pkg)
                } else {
                    Toast.makeText(this@ProcessListActivity, "Enter a package name", Toast.LENGTH_SHORT).show()
                }
            }
        }
        manualRow.addView(attachBtn)
        header.addView(manualRow)

        // ─── Process count + refresh row ───
        val infoRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(0, dp(12), 0, dp(4))
        }

        countLabel = TextView(this).apply {
            text = "LOADING..."
            textSize = 13f
            typeface = Typeface.create("sans-serif-black", Typeface.NORMAL)
            setTextColor(blackColor)
            layoutParams = LinearLayout.LayoutParams(0, -2, 1f)
        }
        infoRow.addView(countLabel)

        val refreshBtn = TextView(this).apply {
            text = "REFRESH"
            textSize = 14f
            setTextColor(blackColor)
            typeface = Typeface.create("sans-serif-black", Typeface.NORMAL)
            setPadding(dp(16), dp(8), dp(16), dp(8))
            background = createNeobrutalistBg(yellowAccent)
            setOnClickListener { 
                background = createNeobrutalistBg(whiteColor)
                postDelayed({ background = createNeobrutalistBg(yellowAccent) }, 100)
                loadProcesses() 
            }
        }
        infoRow.addView(refreshBtn)

        header.addView(infoRow)

        // Divider
        header.addView(View(this).apply {
            setBackgroundColor(blackColor)
            layoutParams = LinearLayout.LayoutParams(-1, dp(4)).apply { topMargin = dp(16) }
        })

        root.addView(header)

        // ════════════════════ PROCESS LIST ════════════════════
        val scrollView = ScrollView(this).apply {
            layoutParams = LinearLayout.LayoutParams(-1, 0, 1f)
            isVerticalScrollBarEnabled = false
            setPadding(dp(24), dp(12), dp(24), dp(24))
        }

        processListLayout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
        }
        scrollView.addView(processListLayout)
        root.addView(scrollView)

        setContentView(root)
        loadProcesses()
    }

    // ═══════════════ DATA ═══════════════

    private fun loadProcesses() {
        allProcesses = try {
            val rawList = DumperCore.GetRunningProcesses()
            rawList.map { entry ->
                val parts = entry.split(":", limit = 2)
                val pid = parts[0].toIntOrNull() ?: 0
                val name = if (parts.size > 1) parts[1] else ""
                pid to name
            }.sortedBy { it.second.lowercase() }
        } catch (e: Exception) {
            emptyList()
        }
        displayProcesses(allProcesses)
    }

    private fun filterProcesses(query: String) {
        val filtered = if (query.isEmpty()) allProcesses
        else allProcesses.filter { it.second.contains(query, ignoreCase = true) }
        displayProcesses(filtered)
    }

    private fun displayProcesses(processes: List<Pair<Int, String>>) {
        processListLayout?.removeAllViews()
        countLabel?.text = "${processes.size} PROCESS${if (processes.size != 1) "ES" else ""} FOUND"

        if (processes.isEmpty()) {
            val empty = LinearLayout(this).apply {
                orientation = LinearLayout.VERTICAL
                gravity = Gravity.CENTER
                setPadding(0, dp(48), 0, dp(48))
            }
            val emptyTitle = TextView(this).apply {
                text = "NO PROCESSES FOUND"
                textSize = 18f
                setTextColor(blackColor)
                gravity = Gravity.CENTER
                typeface = Typeface.create("sans-serif-black", Typeface.NORMAL)
                setPadding(0, 0, 0, dp(8))
            }
            empty.addView(emptyTitle)

            val emptyDesc = TextView(this).apply {
                text = "MAKE SURE YOU HAVE ROOT ACCESS AND\nTHE GAME IS RUNNING"
                textSize = 12f
                setTextColor(blackColor)
                typeface = Typeface.create("sans-serif-medium", Typeface.NORMAL)
                gravity = Gravity.CENTER
            }
            empty.addView(emptyDesc)

            processListLayout?.addView(empty)
            return
        }

        for ((pid, name) in processes) {
            processListLayout?.addView(createProcessCard(pid, name))
        }
    }

    // ═══════════════ CARDS ═══════════════

    private fun createProcessCard(pid: Int, packageName: String): View {
        val card = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(16), dp(16), dp(16), dp(16))
            layoutParams = LinearLayout.LayoutParams(-1, -2).apply {
                topMargin = dp(6)
                bottomMargin = dp(6)
            }
            background = createNeobrutalistBg(whiteColor)
            isClickable = true
            isFocusable = true

            setOnClickListener {
                background = createNeobrutalistBg(yellowAccent)
                postDelayed({
                    background = createNeobrutalistBg(whiteColor)
                }, 200)
                attachToProcess(pid, packageName)
            }
        }

        // Package icon box
        val iconView = TextView(this).apply {
            val initial = packageName.substringAfterLast('.').firstOrNull()?.uppercase() ?: "?"
            text = initial
            textSize = 20f
            setTextColor(blackColor)
            typeface = Typeface.create("sans-serif-black", Typeface.NORMAL)
            gravity = Gravity.CENTER
            val size = dp(44)
            layoutParams = LinearLayout.LayoutParams(size, size).apply { rightMargin = dp(16) }
            background = createNeobrutalistBg(pinkAccent)
        }
        card.addView(iconView)

        // Text
        val textCol = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(0, -2, 1f)
        }

        val nameView = TextView(this).apply {
            text = packageName.uppercase()
            textSize = 14f
            setTextColor(blackColor)
            typeface = Typeface.create("sans-serif-black", Typeface.NORMAL)
            isSingleLine = true
        }
        textCol.addView(nameView)

        val pidView = TextView(this).apply {
            text = "PID $pid"
            textSize = 12f
            setTextColor(blackColor)
            typeface = Typeface.create("sans-serif-medium", Typeface.NORMAL)
            setPadding(0, dp(2), 0, 0)
        }
        textCol.addView(pidView)

        card.addView(textCol)

        return card
    }

    // ═══════════════ ACTIONS ═══════════════

    private fun attachToPackage(packageName: String) {
        val pid = DumperCore.FindProcess(packageName)
        if (pid > 0) {
            attachToProcess(pid, packageName)
        } else {
            Toast.makeText(this, "Process \"$packageName\" not found.\nIs the game running?", Toast.LENGTH_LONG).show()
        }
    }

    private fun attachToProcess(pid: Int, packageName: String) {
        DumperCore.AttachToProcess(pid)
        Toast.makeText(this, "✓ Attached to $packageName (PID: $pid)", Toast.LENGTH_SHORT).show()
        DumperCore.CheckOverlayPermission(this)
    }

    // Helper to create a thick-bordered, hard-shadow box typical of neobrutalism
    private fun createNeobrutalistBg(fillColor: Int): LayerDrawable {
        val shadow = GradientDrawable().apply {
            setColor(blackColor)
            setStroke(dp(3), blackColor)
        }
        val top = GradientDrawable().apply {
            setColor(fillColor)
            setStroke(dp(3), blackColor)
        }

        val layers = arrayOf(shadow, top)
        val layerDrawable = LayerDrawable(layers)
        layerDrawable.setLayerInset(0, dp(4), dp(4), 0, 0)
        layerDrawable.setLayerInset(1, 0, 0, dp(4), dp(4))
        return layerDrawable
    }

    private fun dp(v: Int): Int = (v * resources.displayMetrics.density).toInt()
}
