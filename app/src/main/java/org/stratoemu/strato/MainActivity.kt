/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package org.stratoemu.strato

import android.content.Intent
import android.graphics.Color
import android.net.Uri
import android.os.Bundle
import android.provider.DocumentsContract
import android.view.View
import android.view.inputmethod.InputMethodManager
import androidx.activity.OnBackPressedCallback
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.res.use
import androidx.core.graphics.ColorUtils
import androidx.core.view.WindowCompat
import androidx.core.view.isInvisible
import androidx.core.view.isVisible
import androidx.documentfile.provider.DocumentFile
import androidx.preference.PreferenceManager
import androidx.recyclerview.widget.DefaultItemAnimator
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.snackbar.Snackbar
import dagger.hilt.android.AndroidEntryPoint
import org.stratoemu.strato.adapter.*
import org.stratoemu.strato.data.AppItem
import org.stratoemu.strato.data.AppItemTag
import org.stratoemu.strato.databinding.MainActivityBinding
import org.stratoemu.strato.loader.AppEntry
import org.stratoemu.strato.loader.LoaderResult
import org.stratoemu.strato.provider.DocumentsProvider
import org.stratoemu.strato.settings.AppSettings
import org.stratoemu.strato.settings.EmulationSettings
import org.stratoemu.strato.settings.SettingsActivity
import org.stratoemu.strato.utils.GpuDriverHelper
import org.stratoemu.strato.utils.WindowInsetsHelper
import javax.inject.Inject
import kotlin.math.ceil
import com.google.android.material.R as MaterialR

private fun View.animateDockPress() {
    animate().cancel()
    scaleX = 1f
    scaleY = 1f
    animate().scaleX(0.96f).scaleY(0.96f).setDuration(75L).withEndAction {
        animate().scaleX(1f).scaleY(1f).setDuration(125L).start()
    }.start()
}

@AndroidEntryPoint
class MainActivity : AppCompatActivity() {

    private val binding by lazy { MainActivityBinding.inflate(layoutInflater) }

    @Inject
    lateinit var appSettings : AppSettings

    private val adapter = GenericAdapter()

    private val layoutType get() = LayoutType.values()[appSettings.layoutType]

    private val viewModel by viewModels<MainViewModel>()

    private var appEntries : List<AppEntry>? = null

    enum class SortingOrder {
        AlphabeticalAsc,
        AlphabeticalDesc
    }

    private var selectedGame: AppItem? = null
    private var selectedGameKey: String? = null

    private val documentPicker = registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) {
        it?.let { uri ->
            try {
                contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)

                val usableDirectory = try {
                    val directory = DocumentFile.fromTreeUri(this, uri)
                    directory != null && directory.exists() && directory.isDirectory && directory.canRead()
                } catch (e : Exception) {
                    false
                }

                if (!usableDirectory)
                    throw IllegalArgumentException(getString(R.string.selected_folder_cannot_be_read))

                appSettings.searchLocation = uri.toString()
                loadRoms(false)
            } catch (e : Exception) {
                android.util.Log.e("StratoAdreno", "Failed to use selected ROM directory: $uri", e)
                Snackbar.make(
                    findViewById(android.R.id.content),
                    getString(R.string.error) + ": " + (e.localizedMessage ?: e.javaClass.simpleName),
                    Snackbar.LENGTH_LONG
                ).show()
            }
        }
    }

    private val setupCallback = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
        if (appSettings.searchLocation.isBlank()) finish()
        else loadRoms(false)
    }

    private val settingsCallback = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
        if (appSettings.refreshRequired) loadRoms(false)
    }

    private fun AppItem.toViewItem() = AppViewItem(layoutType, this, ::selectGame, ::selectShowGameDialog) { selectedGameKey == it.key() }

    override fun onCreate(savedInstanceState : Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(binding.root)
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsHelper.applyToActivity(binding.root, binding.appList)
        WindowInsetsHelper.addMargin(binding.libraryDock, consume = false, bottom = true)

        PreferenceManager.setDefaultValues(this, R.xml.app_preferences, false)
        PreferenceManager.setDefaultValues(this, R.xml.emulation_preferences, false)

        // Match system-bar icon contrast to the active light/dark theme.
        val backgroundColor = obtainStyledAttributes(intArrayOf(android.R.attr.colorBackground)).use {
            it.getColor(0, Color.BLACK)
        }
        val useDarkSystemBarIcons = ColorUtils.calculateLuminance(backgroundColor) > 0.5
        WindowCompat.getInsetsController(window, window.decorView).apply {
            isAppearanceLightStatusBars = useDarkSystemBarIcons
            isAppearanceLightNavigationBars = useDarkSystemBarIcons
        }

        binding.appList.setHasFixedSize(true)

        setupAppList()

        binding.swipeRefreshLayout.apply {
            setProgressBackgroundColorSchemeColor(
                obtainStyledAttributes(intArrayOf(MaterialR.attr.colorSurfaceVariant)).use { it.getColor(0, Color.BLACK) })
            setColorSchemeColors(obtainStyledAttributes(intArrayOf(MaterialR.attr.colorPrimary)).use { it.getColor(0, Color.WHITE) })
            post { setDistanceToTriggerSync(binding.swipeRefreshLayout.height / 3) }
            setOnRefreshListener { loadRoms(false) }
        }

        viewModel.stateData.observe(this, ::handleState)
        if (appSettings.searchLocation.isBlank()) {
            if (savedInstanceState == null)
                setupCallback.launch(Intent(this, org.stratoemu.strato.setup.SetupActivity::class.java))
        } else {
            loadRoms(!appSettings.refreshRequired)
        }

        binding.searchBar.apply {
            binding.logIcon.setOnClickListener {
                val file = DocumentFile.fromSingleUri(this@MainActivity, DocumentsContract.buildDocumentUri(DocumentsProvider.AUTHORITY, "${DocumentsProvider.ROOT_ID}/logs/emulation.log"))!!
                if (file.exists() && file.length() != 0L) {
                    val intent = Intent(Intent.ACTION_SEND)
                        .setDataAndType(file.uri, "text/plain")
                        .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                        .putExtra(Intent.EXTRA_STREAM, file.uri)
                    startActivity(Intent.createChooser(intent, getString(R.string.log_share_prompt)))
                } else {
                    Snackbar.make(this@MainActivity.findViewById(android.R.id.content), getString(R.string.logs_not_found), Snackbar.LENGTH_SHORT).show()
                }
            }
            binding.settingsIcon.setOnClickListener { settingsCallback.launch(Intent(context, SettingsActivity::class.java)) }
            binding.searchIcon.setOnClickListener { toggleSearch() }
            addTextChangedListener(afterTextChanged = { editable ->
                editable?.let { text -> adapter.filter.filter(text.toString()) }
            })
        }

        binding.playButton.setOnClickListener { selectedGame?.let(::launchGame) }
        binding.detailsButton.setOnClickListener { selectedGame?.let(::selectShowGameDialog) }

        binding.navGames.setOnClickListener {
            binding.navGames.animateDockPress()
            if (binding.searchBar.isVisible) toggleSearch()
            binding.searchBar.text = ""
            binding.appBarLayout.setExpanded(true)
            binding.appList.smoothScrollToPosition(0)
        }
        // Game details already contain the DLC/Update installation action.
        binding.navInstall.setOnClickListener {
            binding.navInstall.animateDockPress()
            selectedGame?.let(::selectShowGameDialog)
        }
        binding.navAdd.setOnClickListener {
            binding.navAdd.animateDockPress()
            documentPicker.launch(null)
        }
    }

    private fun setAppListDecoration() {
        binding.appList.apply {
            while (itemDecorationCount > 0) removeItemDecorationAt(0)
            when (layoutType) {
                LayoutType.List -> Unit

                LayoutType.Grid, LayoutType.GridCompact -> addItemDecoration(GridSpacingItemDecoration(resources.getDimensionPixelSize(R.dimen.grid_padding)))
            }
        }
    }

    /**
     * This layout manager handles situations where [onFocusSearchFailed] gets called, when possible we always want to focus on the item with the same span index
     */
    private inner class CustomLayoutManager(gridSpan : Int) : GridLayoutManager(this, gridSpan) {
        init {
            spanSizeLookup = object : SpanSizeLookup() {
                override fun getSpanSize(position : Int) = if (layoutType == LayoutType.List || adapter.currentItems[position].fullSpan) gridSpan else 1
            }
        }

        override fun onRequestChildFocus(parent : RecyclerView, state : RecyclerView.State, child : View, focused : View?) : Boolean {
            binding.appBarLayout.setExpanded(false)
            return super.onRequestChildFocus(parent, state, child, focused)
        }

        override fun onFocusSearchFailed(focused : View, focusDirection : Int, recycler : RecyclerView.Recycler, state : RecyclerView.State) : View? {
            val nextFocus = super.onFocusSearchFailed(focused, focusDirection, recycler, state)
            when (focusDirection) {
                View.FOCUS_DOWN -> {
                    return null
                }

                View.FOCUS_UP -> {
                    if (nextFocus?.isFocusable != true) {
                        binding.searchBar.requestFocus()
                        binding.appBarLayout.setExpanded(true)
                        binding.appList.smoothScrollToPosition(0)
                        return null
                    }
                }
            }
            return nextFocus
        }
    }

    private fun setupAppList() {
        binding.appList.adapter = adapter
        binding.appList.itemAnimator = DefaultItemAnimator().apply {
            addDuration = 180L
            removeDuration = 120L
            moveDuration = 180L
            changeDuration = 120L
            supportsChangeAnimations = false
        }

        val itemWidth = 225
        val metrics = resources.displayMetrics
        val gridSpan = ceil((metrics.widthPixels / metrics.density) / itemWidth).toInt()

        binding.appList.layoutManager = CustomLayoutManager(gridSpan)
        setAppListDecoration()


    }

    private fun getAppItems() = mutableListOf<AppViewItem>().apply {
        appEntries?.let { entries ->
            sortGameList(entries.toList()).forEach { entry ->
                add(AppItem(entry).toViewItem())
            }
        }
    }

    private fun sortGameList(gameList : List<AppEntry>) : List<AppEntry> {
        val sortedApps : MutableList<AppEntry> = mutableListOf()
        gameList.forEach { entry ->
            if (!appSettings.filterInvalidFiles || entry.loaderResult != LoaderResult.ParsingError)
                sortedApps.add(entry)
        }
        when (appSettings.sortAppsBy) {
            SortingOrder.AlphabeticalAsc.ordinal -> sortedApps.sortBy { it.name }
            SortingOrder.AlphabeticalDesc.ordinal -> sortedApps.sortByDescending { it.name }
        }
        return sortedApps
    }

    private fun handleState(state : MainState) = when (state) {
        MainState.Loading -> {
            // Pull-to-refresh remains available; the top only has Search and Settings.
            binding.swipeRefreshLayout.isRefreshing = true
        }

        is MainState.Loaded -> {
            binding.swipeRefreshLayout.isRefreshing = false

            appEntries = state.items
            populateAdapter()
        }

        is MainState.Error -> Snackbar.make(findViewById(android.R.id.content), getString(R.string.error) + ": ${state.ex.localizedMessage}", Snackbar.LENGTH_SHORT).show()
    }

    private fun selectGame(appItem : AppItem) {
        if (binding.swipeRefreshLayout.isRefreshing) return
        if (selectedGameKey == appItem.key()) {
            if (appSettings.selectAction) selectShowGameDialog(appItem) else launchGame(appItem)
        } else {
            updateSelectedGame(appItem)
        }
    }

    private fun updateSelectedGame(appItem : AppItem?, refreshGrid : Boolean = true) {
        val previousKey = selectedGameKey
        selectedGame = appItem
        selectedGameKey = appItem?.key()

        val featuredPanel = binding.featuredPanel
        featuredPanel.animate().cancel()
        if (appItem != null) {
            if (!featuredPanel.isVisible) {
                featuredPanel.alpha = 0f
                featuredPanel.translationY = 8f * resources.displayMetrics.density
            }
            featuredPanel.isVisible = true
            featuredPanel.animate().alpha(1f).translationY(0f).setDuration(190L).start()
        } else if (featuredPanel.isVisible) {
            featuredPanel.animate().alpha(0f).translationY(-4f * resources.displayMetrics.density)
                .setDuration(120L).withEndAction {
                    featuredPanel.isVisible = false
                    featuredPanel.alpha = 1f
                    featuredPanel.translationY = 0f
                }.start()
        }
        binding.featuredTitle.text = appItem?.title.orEmpty()
        val playable = appItem?.loaderResult == LoaderResult.Success
        binding.featuredStatus.text = if (playable) getString(R.string.mali_ready)
            else appItem?.loaderResultString(this).orEmpty()
        binding.playButton.isEnabled = playable
        binding.detailsButton.isEnabled = appItem != null
        binding.navInstall.isEnabled = appItem != null
        if (refreshGrid && adapter.itemCount > 0 && previousKey != selectedGameKey) {
            listOfNotNull(previousKey, selectedGameKey).distinct().forEach { key ->
                val position = adapter.currentItems.indexOfFirst { it.key() == key }
                if (position >= 0) adapter.notifyItemChanged(position)
            }
        }
    }

    private fun toggleSearch() {
        val show = !binding.searchBar.isVisible
        binding.searchBar.isVisible = show
        onBackPressedCallback.isEnabled = show
        if (show) {
            binding.appBarLayout.setExpanded(true)
            binding.searchBar.focusSearch()
        } else {
            binding.searchBar.text = ""
            binding.searchBar.clearFocus()
            val keyboard = getSystemService(INPUT_METHOD_SERVICE) as InputMethodManager
            keyboard.hideSoftInputFromWindow(binding.searchBar.windowToken, 0)
        }
    }

    private fun launchGame(appItem : AppItem) {
        if (binding.swipeRefreshLayout.isRefreshing || appItem.loaderResult != LoaderResult.Success) return
        startActivity(Intent(this, EmulationActivity::class.java).apply {
            putExtra(AppItemTag, appItem)
            putExtra(EmulationActivity.ReturnToMainTag, true)
        })
    }

    private fun selectShowGameDialog(appItem : AppItem) {
        if (binding.swipeRefreshLayout.isRefreshing) return

        AppDialog.newInstance(appItem).show(supportFragmentManager, "game")
    }

    private fun loadRoms(loadFromFile : Boolean) {
        if (appSettings.searchLocation.isBlank()) {
            binding.swipeRefreshLayout.isRefreshing = false
            return
        }
        if (!loadFromFile) {
            binding.romPlaceholder.isVisible = true
            binding.romPlaceholder.text = getString(R.string.searching_roms)
        }
        viewModel.loadRoms(this, loadFromFile, Uri.parse(appSettings.searchLocation), EmulationSettings.global.systemLanguage)
        appSettings.refreshRequired = false
    }

    private fun populateAdapter() {
        val games = appEntries?.let(::sortGameList).orEmpty()
        val selectedEntry = games.firstOrNull { AppItem(it).key() == selectedGameKey } ?: games.firstOrNull()
        updateSelectedGame(selectedEntry?.let { AppItem(it) }, refreshGrid = false)

        val items = getAppItems()
        binding.romPlaceholder.isVisible = items.isEmpty()
        if (items.isEmpty())
            binding.romPlaceholder.text = getString(R.string.no_rom)
        adapter.setItems(items)
    }

    private val onBackPressedCallback = object : OnBackPressedCallback(true) {
        override fun handleOnBackPressed() {
            if (binding.searchBar.isVisible) {
                toggleSearch()
            } else {
                isEnabled = false
                onBackPressedDispatcher.onBackPressed()
            }
        }
    }

    override fun onStart() {
        super.onStart()

        binding.searchBar.addTextChangedListener { text ->
            if (!onBackPressedCallback.isEnabled && !text.isNullOrEmpty()) {
                onBackPressedCallback.isEnabled = true
            }
        }

        onBackPressedDispatcher.addCallback(this, onBackPressedCallback)
        onBackPressedCallback.isEnabled = binding.searchBar.hasFocus() || binding.searchBar.text.isNotEmpty()
    }

    override fun onResume() {
        super.onResume()

        // Try to return to normal GPU clocks upon resuming back to main activity, to avoid GPU being stuck at max clocks after a crash
        GpuDriverHelper.forceMaxGpuClocks(false)

        var layoutTypeChanged = false
        for (appViewItem in adapter.allItems.filterIsInstance(AppViewItem::class.java)) {
            if (layoutType != appViewItem.layoutType) {
                appViewItem.layoutType = layoutType
                layoutTypeChanged = true
            } else {
                break
            }
        }

        if (layoutTypeChanged) {
            setAppListDecoration()
            adapter.notifyItemRangeChanged(0, adapter.currentItems.size)
        }

        if (appSettings.searchLocation.isNotBlank()) {
            viewModel.checkRomHash(Uri.parse(appSettings.searchLocation), EmulationSettings.global.systemLanguage)
        }
    }
}
