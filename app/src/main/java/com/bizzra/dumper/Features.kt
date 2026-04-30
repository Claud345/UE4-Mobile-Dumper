package com.bizzra.dumper

import android.R
import android.content.Intent
import android.content.res.ColorStateList
import android.graphics.Color
import android.graphics.PorterDuff
import android.graphics.drawable.GradientDrawable
import android.net.Uri
import android.os.Build
import android.text.Html
import android.view.Gravity
import android.view.View
import android.webkit.WebView
import android.widget.LinearLayout
import android.widget.RadioGroup
import android.widget.SeekBar
import android.widget.SeekBar.OnSeekBarChangeListener
import android.widget.TextView

class Features(val service: FloatingService) {
    var menuChanges: MenuChanges? = null

    var colorBgMenu: Int = Color.parseColor("#FFF4E0") // Cream/Light bg
    var colorBgCard: Int = Color.parseColor("#FFFFFF") // White components
    var colorBgCategory: Int = Color.parseColor("#FFD93D") // Yellow category headers
    var colorStroke: Int = Color.parseColor("#000000") // Solid black outlines

    // --- Text Colors ---
    var colorTextMain: Int = Color.parseColor("#000000") // Solid black text
    var colorTextSub: Int = Color.parseColor("#000000") // Solid black text
    var htmlColorHighlight: String = "#FF6B6B" // Pink highlight

    // --- Component Colors ---
    var colorAccent: Int = Color.parseColor("#FF6B6B") // Pink active states
    var colorInactive: Int = Color.parseColor("#AAAAAA") // Gray inactive
    var colorBtnNormal: Int = Color.parseColor("#FFD93D") // Yellow default buttons

    init {
        menuChanges = MenuChanges(service)
    }

    fun getStyle(color: Int, radius: Float = 0f, strokeWidth: Int = 4, strokeColor: Int = colorStroke): android.graphics.drawable.Drawable {
        // Shadow layer (solid black, offset right and down)
        val shadow = GradientDrawable().apply {
            setColor(colorStroke)
            setStroke(strokeWidth, strokeColor)
            cornerRadius = radius
        }
        // Top layer
        val top = GradientDrawable().apply {
            setColor(color)
            setStroke(strokeWidth, strokeColor)
            cornerRadius = radius
        }

        val layers = arrayOf(shadow, top)
        val layerDrawable = android.graphics.drawable.LayerDrawable(layers)
        // Offset the shadow by 4dp right and down
        val offset = (4 * service.resources.displayMetrics.density).toInt()
        layerDrawable.setLayerInset(0, offset, offset, 0, 0)
        layerDrawable.setLayerInset(1, 0, 0, offset, offset)
        return layerDrawable
    }

    fun getStyle(color: Int, radii: FloatArray, strokeWidth: Int = 4, strokeColor: Int = colorStroke): android.graphics.drawable.Drawable {
        val shadow = GradientDrawable().apply {
            setColor(colorStroke)
            setStroke(strokeWidth, strokeColor)
            cornerRadii = radii
        }
        val top = GradientDrawable().apply {
            setColor(color)
            setStroke(strokeWidth, strokeColor)
            cornerRadii = radii
        }

        val layers = arrayOf(shadow, top)
        val layerDrawable = android.graphics.drawable.LayerDrawable(layers)
        val offset = (4 * service.resources.displayMetrics.density).toInt()
        layerDrawable.setLayerInset(0, offset, offset, 0, 0)
        layerDrawable.setLayerInset(1, 0, 0, offset, offset)
        return layerDrawable
    }

    fun Switch(featNum: Int, featName: String?, swiOn: Boolean): View {
        val container = LinearLayout(service).apply {
            layoutParams = LinearLayout.LayoutParams(-1, -2).apply { setMargins(15, 10, 15, 15) }
            background = getStyle(colorBgCard, 0f, 4, colorStroke)
            setPadding(20, 15, 20, 15)
        }
        val switch = android.widget.Switch(service).apply {
            text = featName
            isChecked = swiOn
            setTextColor(colorTextSub)
            textSize = 15f
            layoutParams = LinearLayout.LayoutParams(-1, -2)

            val states = arrayOf(
                intArrayOf(-R.attr.state_enabled),
                intArrayOf(R.attr.state_checked),
                intArrayOf()
            )
            val colors = intArrayOf(Color.DKGRAY, colorAccent, colorInactive)
            thumbTintList = ColorStateList(states, colors)
            trackTintList = ColorStateList(states, intArrayOf(Color.DKGRAY, Color.parseColor("#4400E5FF"), Color.parseColor("#44555555")))

            setOnCheckedChangeListener { _, bool ->
                menuChanges?.changeFeatureBool(featName, featNum, bool)
            }
        }
        container.addView(switch)
        return container
    }

    fun SeekBar(featNum: Int, featName: String?, min: Int, max: Int): View {
        val container = LinearLayout(service).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(-1, -2).apply { setMargins(15, 10, 15, 10) }
            background = getStyle(colorBgCard, 20f, 2, colorStroke)
            setPadding(20, 20, 20, 20)
        }

        val textView = TextView(service).apply {
            text = Html.fromHtml("<b>$featName:</b> <font color='$htmlColorHighlight'>$min</font>")
            setTextColor(colorTextSub)
            textSize = 14f
            setPadding(10, 0, 0, 10)
        }

        val seekBar = SeekBar(service).apply {
            setMax(max)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) setMin(min)
            progress = min
            setPadding(25, 10, 25, 10)

            progressDrawable.setColorFilter(colorAccent, PorterDuff.Mode.SRC_ATOP)
            thumb.setColorFilter(colorAccent, PorterDuff.Mode.SRC_ATOP)

            setOnSeekBarChangeListener(object : OnSeekBarChangeListener {
                override fun onProgressChanged(s: SeekBar?, i: Int, z: Boolean) {
                    val p = if (i < min) min else i
                    menuChanges?.changeFeatureInt(featName, featNum, p)
                    textView.text = Html.fromHtml("<b>$featName:</b> <font color='$htmlColorHighlight'>$p</font>")
                }

                override fun onStartTrackingTouch(s: SeekBar?) {}
                override fun onStopTrackingTouch(s: SeekBar?) {}
            })
        }

        container.addView(textView)
        container.addView(seekBar)
        return container
    }

    fun Button(featNum: Int, featName: String?): View {
        return android.widget.Button(service).apply {
            text = Html.fromHtml("<b>$featName</b>")
            setTextColor(colorTextSub)
            isAllCaps = false
            textSize = 14f
            background = getStyle(colorBtnNormal, 0f, 4, colorStroke)

            layoutParams = LinearLayout.LayoutParams(-1, -2).apply {
                setMargins(15, 10, 15, 15)
            }

            setOnClickListener {
                menuChanges?.changeFeatureInt(featName, featNum, 0)
                background = getStyle(colorBgCard, 0f, 4, colorStroke)
                postDelayed({ background = getStyle(colorBtnNormal, 0f, 4, colorStroke) }, 150)
            }
        }
    }

    fun ButtonLink(featName: String?, url: String?): View {
        return android.widget.Button(service).apply {
            text = Html.fromHtml("🔗 <b>$featName</b>")
            setTextColor(Color.BLACK)
            isAllCaps = false
            textSize = 14f
            background = getStyle(colorAccent, 0f, 4, colorStroke)

            layoutParams = LinearLayout.LayoutParams(-1, -2).apply {
                setMargins(15, 10, 15, 15)
            }

            setOnClickListener {
                val intent = Intent(Intent.ACTION_VIEW)
                intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK
                intent.data = Uri.parse(url)
                service.startActivity(intent)
            }
        }
    }

    fun ButtonOnOff(featNum: Int, featName: String, switchedOn: Boolean): View {
        val finalfeatName = featName.replace("OnOff_", "")
        return android.widget.Button(service).apply {
            setTextColor(if (switchedOn) Color.BLACK else colorTextSub)
            isAllCaps = false
            textSize = 14f
            text = Html.fromHtml("<b>$finalfeatName:</b> ${if (switchedOn) "ON" else "OFF"}")
            background =
                getStyle(if (switchedOn) colorAccent else colorBtnNormal, 0f, 4, colorStroke)

            layoutParams = LinearLayout.LayoutParams(-1, -2).apply {
                setMargins(15, 10, 15, 15)
            }

            setOnClickListener {
                val newState = text.toString().endsWith("OFF")
                menuChanges?.changeFeatureBool(finalfeatName, featNum, newState)
                text = Html.fromHtml("<b>$finalfeatName:</b> ${if (newState) "ON" else "OFF"}")
                setTextColor(if (newState) Color.BLACK else colorTextSub)
                background = getStyle(if (newState) colorAccent else colorBtnNormal, 0f, 4, colorStroke)
            }
        }
    }

    fun Spinner(featNum: Int, featName: String?, list: String): View {
        val container = LinearLayout(service).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(-1, -2).apply { setMargins(15, 10, 15, 15) }
            background = getStyle(colorBgCard, 0f, 4, colorStroke)
            setPadding(20, 20, 20, 20)
        }

        val textView = TextView(service).apply {
            text = Html.fromHtml("<b>$featName</b>")
            setTextColor(colorTextSub)
            textSize = 14f
            setPadding(10, 0, 0, 10)
        }

        val spinner = android.widget.Spinner(service).apply {
            val items = list.split(",").filter { it.isNotEmpty() }
            val adapter = android.widget.ArrayAdapter(context, R.layout.simple_spinner_item, items)
            adapter.setDropDownViewResource(R.layout.simple_spinner_dropdown_item)
            this.adapter = adapter

            setPadding(10, 10, 10, 10)
            background = getStyle(colorBgMenu, 0f, 4, colorStroke)

            onItemSelectedListener = object : android.widget.AdapterView.OnItemSelectedListener {
                override fun onItemSelected(parent: android.widget.AdapterView<*>?, view: View?, position: Int, id: Long) {
                    menuChanges?.changeFeatureInt(featName, featNum, position)
                    (view as? TextView)?.setTextColor(colorTextMain)
                }

                override fun onNothingSelected(parent: android.widget.AdapterView<*>?) {}
            }
        }

        container.addView(textView)
        container.addView(spinner)
        return container
    }

    fun InputField(featNum: Int, featName: String?, defaultVal: String): View {
        val container = LinearLayout(service).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(-1, -2).apply { setMargins(15, 10, 15, 15) }
            background = getStyle(colorBgCard, 0f, 4, colorStroke)
            setPadding(20, 20, 20, 20)
        }

        val textView = TextView(service).apply {
            text = Html.fromHtml("<b>$featName</b>")
            setTextColor(colorTextSub)
            textSize = 14f
            setPadding(10, 0, 0, 10)
        }

        val editText = android.widget.EditText(service).apply {
            setText(defaultVal)
            setTextColor(colorTextMain)
            setHintTextColor(colorTextSub)
            textSize = 14f
            setPadding(20, 20, 20, 20)
            background = getStyle(colorBgMenu, 0f, 4, colorStroke)
            layoutParams = LinearLayout.LayoutParams(-1, -2)

            setOnEditorActionListener { v, _, _ ->
                menuChanges?.changeFeatureString(featName, featNum, v.text.toString())
                false
            }
        }

        container.addView(textView)
        container.addView(editText)
        return container
    }

    fun InputText(featNum: Int, featName: String?): View {
        val container = LinearLayout(service).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(-1, -2).apply { setMargins(15, 10, 15, 15) }
            background = getStyle(colorBgCard, 0f, 4, colorStroke)
            setPadding(20, 20, 20, 20)
        }

        val textView = TextView(service).apply {
            text = Html.fromHtml("<b>$featName</b>")
            setTextColor(colorTextSub)
            textSize = 14f
            setPadding(10, 0, 0, 10)
        }

        val editText = android.widget.EditText(service).apply {
            hint = "Enter text..."
            setTextColor(colorTextMain)
            setHintTextColor(colorInactive)
            textSize = 14f
            setPadding(20, 20, 20, 20)
            background = getStyle(colorBgMenu, 0f, 4, colorStroke)
            inputType = android.text.InputType.TYPE_CLASS_TEXT
            layoutParams = LinearLayout.LayoutParams(-1, -2)

            setOnEditorActionListener { v, _, _ ->
                menuChanges?.changeFeatureString(featName, featNum, v.text.toString())
                false
            }
        }

        container.addView(textView)
        container.addView(editText)
        return container
    }

    fun InputNum(featNum: Int, featName: String?, defaultVal: Int): View {
        val container = LinearLayout(service).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(-1, -2).apply { setMargins(15, 10, 15, 15) }
            background = getStyle(colorBgCard, 0f, 4, colorStroke)
            setPadding(20, 20, 20, 20)
        }

        val textView = TextView(service).apply {
            text = Html.fromHtml("<b>$featName</b>")
            setTextColor(colorTextSub)
            textSize = 14f
            setPadding(10, 0, 0, 10)
        }

        val editText = android.widget.EditText(service).apply {
            setText(defaultVal.toString())
            setTextColor(colorTextMain)
            setHintTextColor(colorInactive)
            textSize = 14f
            setPadding(20, 20, 20, 20)
            background = getStyle(colorBgMenu, 0f, 4, colorStroke)
            inputType =
                android.text.InputType.TYPE_CLASS_NUMBER or android.text.InputType.TYPE_NUMBER_FLAG_SIGNED
            layoutParams = LinearLayout.LayoutParams(-1, -2)

            setOnEditorActionListener { v, _, _ ->
                try {
                    menuChanges?.changeFeatureInt(featName, featNum, v.text.toString().toInt())
                } catch (e: Exception) { }
                false
            }
        }

        container.addView(textView)
        container.addView(editText)
        return container
    }

    fun InputLNum(featNum: Int, featName: String?, defaultVal: Long): View {
        val container = LinearLayout(service).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(-1, -2).apply { setMargins(15, 10, 15, 15) }
            background = getStyle(colorBgCard, 0f, 4, colorStroke)
            setPadding(20, 20, 20, 20)
        }

        val textView = TextView(service).apply {
            text = Html.fromHtml("<b>$featName</b>")
            setTextColor(colorTextSub)
            textSize = 14f
            setPadding(10, 0, 0, 10)
        }

        val editText = android.widget.EditText(service).apply {
            setText(defaultVal.toString())
            setTextColor(colorTextMain)
            setHintTextColor(colorInactive)
            textSize = 14f
            setPadding(20, 20, 20, 20)
            background = getStyle(colorBgMenu, 0f, 4, colorStroke)
            inputType =
                android.text.InputType.TYPE_CLASS_NUMBER or android.text.InputType.TYPE_NUMBER_FLAG_SIGNED
            layoutParams = LinearLayout.LayoutParams(-1, -2)

            setOnEditorActionListener { v, _, _ ->
                try {
                    menuChanges?.changeFeatureInt(featName, featNum, v.text.toString().toInt())
                } catch (e: Exception) { }
                false
            }
        }

        container.addView(textView)
        container.addView(editText)
        return container
    }

    fun CheckBox(featNum: Int, featName: String?, switchedOn: Boolean): View {
        val container = LinearLayout(service).apply {
            layoutParams = LinearLayout.LayoutParams(-1, -2).apply { setMargins(15, 10, 15, 15) }
            background = getStyle(colorBgCard, 0f, 4, colorStroke)
            setPadding(15, 10, 15, 10)
        }
        val checkBox = android.widget.CheckBox(service).apply {
            text = featName
            setTextColor(colorTextSub)
            textSize = 15f
            isChecked = switchedOn
            buttonTintList = ColorStateList.valueOf(colorAccent)
            setOnCheckedChangeListener { _, isChecked ->
                menuChanges?.changeFeatureBool(featName, featNum, isChecked)
            }
        }
        container.addView(checkBox)
        return container
    }

    fun RadioButton(featNum: Int, featName: String?, list: String): View {
        val lists = list.split(",").filter { it.isNotEmpty() }

        val container = LinearLayout(service).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(-1, -2).apply { setMargins(15, 10, 15, 15) }
            background = getStyle(colorBgCard, 0f, 4, colorStroke)
            setPadding(20, 20, 20, 20)
        }

        val textView = TextView(service).apply {
            text = Html.fromHtml("<b>$featName</b>")
            setTextColor(colorTextSub)
            textSize = 15f
            setPadding(0, 0, 0, 10)
        }

        val radioGroup = RadioGroup(service).apply {
            orientation = LinearLayout.VERTICAL
        }

        for (i in lists.indices) {
            val radioBtn = android.widget.RadioButton(service).apply {
                text = lists[i]
                setTextColor(colorTextSub)
                textSize = 14f
                setPadding(10, 5, 10, 5)
                buttonTintList = ColorStateList.valueOf(colorAccent)
                setOnClickListener {
                    textView.text = Html.fromHtml("<b>$featName:</b> <font color='$htmlColorHighlight'>${lists[i]}</font>")
                    menuChanges?.changeFeatureInt(featName, featNum, i)
                }
            }
            radioGroup.addView(radioBtn)
        }

        container.addView(textView)
        container.addView(radioGroup)
        return container
    }

    fun Collapse(linLayout: LinearLayout, text: String?) {
        val collapseContainer = LinearLayout(service).apply {
            layoutParams = LinearLayout.LayoutParams(-1, -2).apply { setMargins(15, 15, 15, 15) }
            orientation = LinearLayout.VERTICAL
            background = getStyle(colorBgCard, 0f, 4, colorStroke)
        }

        val collapseSub = LinearLayout(service).apply {
            orientation = LinearLayout.VERTICAL
            visibility = View.GONE
            setPadding(0, 10, 0, 10)
        }
        service.activeCollapseLayout = collapseSub

        val headerView = TextView(service).apply {
            this.text = Html.fromHtml("<b>$text</b>   ▼")
            gravity = Gravity.CENTER
            setTextColor(colorTextMain)
            textSize = 15f
            setPadding(0, 25, 0, 25)
            background = getStyle(colorBgCategory, 0f, 4, colorStroke)

            var isExpanded = false
            setOnClickListener {
                isExpanded = !isExpanded
                if (isExpanded) {
                    collapseSub.visibility = View.VISIBLE
                    this.text = Html.fromHtml("<b>$text</b>   ▲")
                } else {
                    collapseSub.visibility = View.GONE
                    this.text = Html.fromHtml("<b>$text</b>   ▼")
                }
            }
        }

        collapseContainer.addView(headerView)
        collapseContainer.addView(collapseSub)
        linLayout.addView(collapseContainer)
    }

    fun Category(texts: String?): View {
        return TextView(service).apply {
            text = Html.fromHtml("<b>$texts</b>")
            setTextColor(Color.WHITE)
            gravity = Gravity.CENTER
            textSize = 16f
            layoutParams = LinearLayout.LayoutParams(-1, -2).apply { setMargins(15, 25, 15, 15) }
            setPadding(0, 20, 0, 20)

            background = getStyle(colorBgCategory, 0f, 4, colorStroke)
        }
    }

    fun RichTextView(text: String?): View {
        return TextView(service).apply {
            this.text = Html.fromHtml(text)
            setTextColor(colorTextSub)
            textSize = 14f
            setPadding(25, 10, 25, 10)
        }
    }

    fun RichWebView(text: String): View {
        return WebView(service).apply {
            loadData(text, "text/html", "utf-8")
            setBackgroundColor(Color.TRANSPARENT)
            setPadding(15, 5, 15, 5)
        }
    }

}