/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package org.stratoemu.strato.applet.swkbd

import android.annotation.SuppressLint
import android.content.Context
import android.content.DialogInterface
import android.os.Bundle
import android.text.InputType
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputMethodManager
import androidx.core.widget.doOnTextChanged
import androidx.fragment.app.DialogFragment
import org.stratoemu.strato.databinding.KeyboardDialogBinding
import org.stratoemu.strato.utils.parcelable
import org.stratoemu.strato.utils.stringFromChars

class SoftwareKeyboardDialog : DialogFragment() {
    private val config by lazy { requireArguments().parcelable<SoftwareKeyboardConfig>("config")!! }
    private val initialText by lazy { requireArguments().getString("initialText")!! }
    private val session = SoftwareKeyboardSession()

    companion object {
        /**
         * @param config Holds the [SoftwareKeyboardConfig] that will be used to create the keyboard  between instances of this dialog
         * @param initialText Holds the text that was set by the guest when the dialog was created
         */
        fun newInstance(config : SoftwareKeyboardConfig, initialText : String) : SoftwareKeyboardDialog {
            val args = Bundle()
            args.putParcelable("config", config)
            args.putString("initialText", initialText)
            val fragment = SoftwareKeyboardDialog()
            fragment.arguments = args
            return fragment
        }

        const val validationConfirm = 2
        const val validationError = 1
    }

    private lateinit var binding : KeyboardDialogBinding

    override fun onCreate(savedInstanceState : Bundle?) {
        super.onCreate(savedInstanceState)
        isCancelable = !config.isCancelButtonDisabled
    }

    override fun onCreateView(inflater : LayoutInflater, container : ViewGroup?, savedInstanceState : Bundle?) =
        KeyboardDialogBinding.inflate(inflater, container, false).also { binding = it }.root

    @SuppressLint("SetTextI18n")
    override fun onViewCreated(view : View, savedInstanceState : Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        if (config.inputFormMode == InputFormMode.OneLine) {
            binding.header.text = stringFromChars(config.headerText)
            binding.header.visibility = View.VISIBLE
            binding.sub.text = stringFromChars(config.subText)
            binding.sub.visibility = View.VISIBLE
        }

        if (config.keyboardMode == KeyboardMode.Numeric) {
            binding.textInput.inputType = if (config.passwordMode == PasswordMode.Hide) InputType.TYPE_CLASS_NUMBER or InputType.TYPE_NUMBER_VARIATION_PASSWORD else InputType.TYPE_CLASS_NUMBER
        } else {
            binding.textInput.inputType = if (config.passwordMode == PasswordMode.Hide) InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD else InputType.TYPE_CLASS_TEXT
            if (config.invalidCharsFlags.outsideOfDownloadCode)
                binding.textInput.inputType = binding.textInput.inputType or InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS
            if (config.isUseNewLine)
                binding.textInput.inputType = binding.textInput.inputType or InputType.TYPE_TEXT_FLAG_MULTI_LINE
            else
                binding.textInput.inputType = binding.textInput.inputType and InputType.TYPE_TEXT_FLAG_MULTI_LINE.inv()
        }

        val okText = stringFromChars(config.okText)
        if (okText.isNotBlank())
            binding.okButton.text = okText
        val guideText = stringFromChars(config.guideText)
        if (guideText.isNotBlank())
            binding.inputLayout.hint = guideText

        binding.textInput.filters = arrayOf(SoftwareKeyboardFilter(config))
        binding.textInput.setText(initialText)
        binding.textInput.setSelection(if (config.initialCursorPos == InitialCursorPos.First) 0 else binding.textInput.length())
        binding.textInput.doOnTextChanged { text, _, _, _ ->
            binding.okButton.isEnabled = config.isValid(text!!)
            binding.lengthStatus.text = "${text.length}/${config.textMaxLength}"
        }
        binding.lengthStatus.text = "${binding.textInput.length()}/${config.textMaxLength}"
        binding.okButton.isEnabled = config.isValid(binding.textInput.text.toString())
        binding.okButton.setOnClickListener { submit(false) }
        binding.cancelButton.visibility = if (config.isCancelButtonDisabled) View.GONE else View.VISIBLE
        binding.cancelButton.setOnClickListener { submit(true) }
        if (!config.isUseNewLine) {
            binding.textInput.imeOptions = binding.textInput.imeOptions or EditorInfo.IME_ACTION_DONE
            binding.textInput.setOnEditorActionListener { _, action, _ ->
                if (action == EditorInfo.IME_ACTION_DONE) {
                    submit(false)
                    true
                } else false
            }
        }
    }

    private fun submit(cancelled : Boolean) {
        val text = binding.textInput.text.toString()
        if (!cancelled && !config.isValid(text)) return
        if (session.submit(SoftwareKeyboardResult(cancelled, text))) {
            binding.okButton.isEnabled = false
            binding.cancelButton.isEnabled = false
            binding.textInput.isEnabled = false
            (requireContext().getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager)
                .hideSoftInputFromWindow(binding.textInput.windowToken, 0)
        }
    }

    private fun showIme() {
        binding.textInput.requestFocus()
        binding.textInput.post {
            if (isAdded && binding.textInput.isEnabled) {
                (requireContext().getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager)
                    .showSoftInput(binding.textInput, InputMethodManager.SHOW_IMPLICIT)
            }
        }
    }

    override fun onStart() {
        super.onStart()
        dialog?.window?.apply {
            setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
            setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE or WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE)
        }
        dialog?.setCanceledOnTouchOutside(false)
        showIme()
    }

    fun prepareForRetry() {
        session.retry()
        if (!isAdded) return
        binding.textInput.isEnabled = true
        binding.okButton.isEnabled = config.isValid(binding.textInput.text.toString())
        binding.cancelButton.isEnabled = !config.isCancelButtonDisabled
        showIme()
    }

    override fun onCancel(dialog : DialogInterface) {
        session.close()
        super.onCancel(dialog)
    }

    override fun onDestroy() {
        session.close()
        super.onDestroy()
    }

    fun cancelForShutdown() = session.close()

    fun waitForSubmitOrCancel() : SoftwareKeyboardResult = session.awaitResult()
}
