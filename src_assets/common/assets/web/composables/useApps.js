import { ref, computed } from 'vue'
import { AppService } from '../services/appService.js'
import { APP_CONSTANTS, ENV_VARS_CONFIG } from '../utils/constants.js'
import { debounce, deepClone } from '../utils/helpers.js'
import { trackEvents } from '../config/firebase.js'
import { searchCoverImage, batchSearchCoverImages } from '../utils/coverSearch.js'

const MESSAGE_DURATION = 3000

/**
 * Apps management composable
 */
export function useApps() {
  // State
  const apps = ref([])
  const originalApps = ref([])
  const filteredApps = ref([])
  const searchQuery = ref('')
  const editingApp = ref(null)
  const platform = ref('')
  const isSaving = ref(false)
  const isDragging = ref(false)
  const viewMode = ref('grid')
  const message = ref('')
  const messageType = ref('success')
  const envVars = ref({})
  const debouncedSearch = ref(null)
  const isScanning = ref(false)
  const scannedApps = ref([])
  const showScanResult = ref(false)
  const scannedAppsSearchQuery = ref('')
  const showGamesOnly = ref(false)
  const selectedAppType = ref('all') // 'all', 'executable', 'shortcut', 'batch', 'command', 'url'
  const deleteConfirmIndex = ref(null)

  // Computed properties
  const messageClass = computed(() => ({
    [`alert-${messageType.value}`]: true,
  }))

  // Message icon mapping
  const MESSAGE_ICONS = {
    success: 'fa-check-circle',
    error: 'fa-exclamation-circle',
    warning: 'fa-exclamation-triangle',
    info: 'fa-info-circle',
  }

  const showMessage = (msg, type = APP_CONSTANTS.MESSAGE_TYPES.SUCCESS) => {
    message.value = msg
    messageType.value = type
    setTimeout(() => {
      message.value = ''
    }, MESSAGE_DURATION)
  }

  const getMessageIcon = () => MESSAGE_ICONS[messageType.value] || MESSAGE_ICONS.success

  const createDefaultApp = (overrides = {}) => ({
    ...APP_CONSTANTS.DEFAULT_APP,
    index: -1,
    ...overrides,
  })

  // Initialization
  const init = (t) => {
    envVars.value = Object.fromEntries(
      Object.entries(ENV_VARS_CONFIG).map(([key, translationKey]) => [key, t(translationKey)])
    )
    debouncedSearch.value = debounce(performSearch, APP_CONSTANTS.SEARCH_DEBOUNCE_TIME)
  }

  // Data loading
  const loadApps = async () => {
    try {
      apps.value = await AppService.getApps()
      originalApps.value = deepClone(apps.value)
      filteredApps.value = [...apps.value]
    } catch (error) {
      console.error('Failed to load apps:', error)
      showMessage('Failed to load apps', APP_CONSTANTS.MESSAGE_TYPES.ERROR)
    }
  }

  const loadPlatform = async () => {
    try {
      platform.value = await AppService.getPlatform()
    } catch (error) {
      console.error('Failed to load platform info:', error)
      platform.value = APP_CONSTANTS.PLATFORMS.WINDOWS
    }
  }

  // Search
  const performSearch = () => {
    filteredApps.value = AppService.searchApps(apps.value, searchQuery.value)
  }

  const clearSearch = () => {
    searchQuery.value = ''
    performSearch()
  }

  // App operations
  const getOriginalIndex = (app) => apps.value.indexOf(app)

  const newApp = () => {
    trackEvents.userAction('new_app_clicked')
    editingApp.value = createDefaultApp()
  }

  const editApp = (index) => {
    editingApp.value = { ...deepClone(apps.value[index]), index }
  }

  const closeAppEditor = () => {
    editingApp.value = null
  }

  const handleSaveApp = async (appData) => {
    try {
      isSaving.value = true
      await AppService.saveApps(apps.value, appData)
      await loadApps()
      editingApp.value = null
      showMessage('App saved successfully', APP_CONSTANTS.MESSAGE_TYPES.SUCCESS)
    } catch (error) {
      console.error('Failed to save app:', error)
      showMessage('Failed to save app', APP_CONSTANTS.MESSAGE_TYPES.ERROR)
    } finally {
      isSaving.value = false
    }
  }

  const showDeleteForm = (index) => {
    deleteConfirmIndex.value = index
  }

  const cancelDeleteApp = () => {
    deleteConfirmIndex.value = null
  }

  const confirmDeleteApp = async () => {
    const index = deleteConfirmIndex.value
    if (index === null) return
    deleteConfirmIndex.value = null
    await deleteApp(index)
  }

  const deleteApp = async (index) => {
    const appName = apps.value[index]?.name || 'unknown'
    try {
      apps.value.splice(index, 1)
      await AppService.saveApps(apps.value, null)
      await loadApps()
      showMessage('App deleted successfully', APP_CONSTANTS.MESSAGE_TYPES.SUCCESS)
      trackEvents.appDeleted(appName)
    } catch (error) {
      console.error('Failed to delete app:', error)
      showMessage('Failed to delete app', APP_CONSTANTS.MESSAGE_TYPES.ERROR)
    }
  }

  // Detect whether there are unsaved changes
  const hasUnsavedChanges = () => {
    if (apps.value.length !== originalApps.value.length) {
      return true
    }

    // Deep compare app lists
    const appsStr = JSON.stringify(apps.value.map(app => ({ ...app, index: undefined })))
    const originalStr = JSON.stringify(originalApps.value.map(app => ({ ...app, index: undefined })))

    return appsStr !== originalStr
  }

  const save = async () => {
    // If nothing changed, return early
    if (!hasUnsavedChanges()) {
      showMessage('No changes to save', APP_CONSTANTS.MESSAGE_TYPES.INFO)
      return
    }

    try {
      isSaving.value = true
      await AppService.saveApps(apps.value, null)
      // Update the original list after a successful save
      originalApps.value = deepClone(apps.value)
      showMessage('App list saved successfully', APP_CONSTANTS.MESSAGE_TYPES.SUCCESS)
      trackEvents.userAction('apps_saved', { count: apps.value.length })
    } catch (error) {
      console.error('Failed to save app list:', error)
      showMessage('Failed to save app list', APP_CONSTANTS.MESSAGE_TYPES.ERROR)
    } finally {
      isSaving.value = false
    }
  }

  // Drag-and-drop sorting
  const onDragStart = () => {
    isDragging.value = true
  }

  const onDragEnd = async () => {
    isDragging.value = false
    await save()
  }

  // Cover search (uses the shared coverSearch module)

  // Tauri environment detection
  const isTauriEnv = () => !!window.__TAURI__?.core?.invoke

  // Directory scanning feature
  const scanDirectory = async (extractIcons = true) => {
    const tauri = window.__TAURI__
    if (!tauri?.core?.invoke) {
      showMessage('Scanning is only available in the Tauri environment', APP_CONSTANTS.MESSAGE_TYPES.WARNING)
      return
    }

    if (!tauri?.dialog?.open) {
      showMessage('Unable to open file dialog', APP_CONSTANTS.MESSAGE_TYPES.ERROR)
      return
    }

    try {
      const selectedDir = await tauri.dialog.open({
        directory: true,
        multiple: false,
        title: 'Select directory to scan',
      })

      if (!selectedDir) return

      isScanning.value = true
      showMessage('Scanning directory...', APP_CONSTANTS.MESSAGE_TYPES.INFO)

      const foundApps = await tauri.core.invoke('scan_directory_for_apps', {
        directory: selectedDir,
        extractIcons,
      })

      if (foundApps.length === 0) {
        scannedApps.value = []
        showScanResult.value = true
        showMessage('No applications found that can be added', APP_CONSTANTS.MESSAGE_TYPES.INFO)
      } else {
        // Show scan results immediately (without covers)
        scannedApps.value = foundApps
        showScanResult.value = true
        showMessage(`Found ${foundApps.length} application(s); searching for covers...`, APP_CONSTANTS.MESSAGE_TYPES.INFO)

        // Update cover images asynchronously
        asyncUpdateCovers(foundApps)
      }

      trackEvents.userAction('directory_scanned', { count: foundApps.length, extractIcons })
    } catch (error) {
      console.error('Failed to scan directory:', error)
      showMessage(`Scan failed: ${error}`, APP_CONSTANTS.MESSAGE_TYPES.ERROR)
    } finally {
      isScanning.value = false
    }
  }

  // Scan game platform libraries (Steam/Epic/GOG)
  const scanGameLibraries = async () => {
    const tauri = window.__TAURI__
    if (!tauri?.core?.invoke) {
      showMessage('Scanning is only available in the Tauri environment', APP_CONSTANTS.MESSAGE_TYPES.WARNING)
      return
    }

    try {
      isScanning.value = true
      showMessage('Scanning game platform libraries...', APP_CONSTANTS.MESSAGE_TYPES.INFO)

      const result = await tauri.core.invoke('scan_game_libraries')

      // Convert PlatformGame entries into the scannedApps format
      const steamGames = result.steam || []
      const epicGames = result.epic || []
      const gogGames = result.gog || []
      const allGames = [...steamGames, ...epicGames, ...gogGames]

      if (allGames.length === 0) {
        scannedApps.value = []
        showScanResult.value = true
        showMessage('No installed games detected', APP_CONSTANTS.MESSAGE_TYPES.INFO)
      } else {
        const mapped = allGames.map((game) => ({
          name: game.name,
          cmd: game.cmd,
          'working-dir': game['working-dir'] || game.working_dir || '',
          'image-path': game['cover-url'] || game.cover_url || '',
          source_path: game.install_dir,
          'app-type': game.platform,
          'is-game': true,
        }))

        scannedApps.value = mapped
        showScanResult.value = true

        const parts = []
        if (steamGames.length) parts.push(`Steam ${steamGames.length}`)
        if (epicGames.length) parts.push(`Epic ${epicGames.length}`)
        if (gogGames.length) parts.push(`GOG ${gogGames.length}`)
        showMessage(
          `Found ${result.total ?? allGames.length} game(s) (${parts.join(', ')}) in ${result.scan_time_ms ?? 0}ms`,
          APP_CONSTANTS.MESSAGE_TYPES.SUCCESS
        )
      }

      trackEvents.userAction('game_libraries_scanned', {
        steam: steamGames.length,
        epic: epicGames.length,
        gog: gogGames.length,
        total: result.total ?? allGames.length,
      })
    } catch (error) {
      console.error('Failed to scan game libraries:', error)
      showMessage(`Failed to scan game libraries: ${error}`, APP_CONSTANTS.MESSAGE_TYPES.ERROR)
    } finally {
      isScanning.value = false
    }
  }

  // Update cover images asynchronously
  const asyncUpdateCovers = async (appList) => {
    let coversFound = 0
    const total = appList.length

    // Search all covers in parallel but update the UI one at a time
    const promises = appList.map(async (app, index) => {
      try {
        const imagePath = await searchCoverImage(encodeURIComponent(app.name))
        if (imagePath && scannedApps.value[index]) {
          // Update the cover for the matching position
          scannedApps.value[index] = { ...scannedApps.value[index], 'image-path': imagePath }
          coversFound++
        }
      } catch (error) {
        console.warn(`Failed to search cover: ${app.name}`, error)
      }
    })

    await Promise.allSettled(promises)

    // Show the result once searching is complete
    showMessage(
      `Matched ${coversFound}/${total} cover(s)`,
      coversFound > 0 ? APP_CONSTANTS.MESSAGE_TYPES.SUCCESS : APP_CONSTANTS.MESSAGE_TYPES.INFO
    )
  }

  // Scanned-app field handling
  const getScannedAppField = (app, field) => app[field] || app[field.replace(/-/g, '_')] || ''

  const getScannedAppImage = (app) => getScannedAppField(app, 'image-path')

  const createAppFromScanned = (scannedApp) => ({
    ...APP_CONSTANTS.DEFAULT_APP,
    name: scannedApp.name,
    cmd: scannedApp.cmd,
    'working-dir': getScannedAppField(scannedApp, 'working-dir'),
    'image-path': getScannedAppField(scannedApp, 'image-path'),
  })

  const removeFromScannedList = (sourcePath) => {
    const index = scannedApps.value.findIndex((a) => a.source_path === sourcePath)
    if (index !== -1) {
      scannedApps.value.splice(index, 1)
      if (scannedApps.value.length === 0) {
        showScanResult.value = false
      }
    }
  }

  const addScannedApp = (scannedApp) => {
    editingApp.value = createDefaultApp({
      name: scannedApp.name,
      cmd: scannedApp.cmd,
      'working-dir': getScannedAppField(scannedApp, 'working-dir'),
      'image-path': getScannedAppField(scannedApp, 'image-path'),
    })

    removeFromScannedList(scannedApp.source_path)
    showMessage(`Editing app: ${scannedApp.name}`, APP_CONSTANTS.MESSAGE_TYPES.INFO)
    trackEvents.userAction('scanned_app_edit', { name: scannedApp.name })
  }

  const quickAddScannedApp = async (scannedApp, index) => {
    try {
      apps.value.push(createAppFromScanned(scannedApp))
      await AppService.saveApps(apps.value, null)
      await loadApps()

      scannedApps.value.splice(index, 1)
      if (scannedApps.value.length === 0) {
        showScanResult.value = false
      }

      showMessage(`Added app: ${scannedApp.name}`, APP_CONSTANTS.MESSAGE_TYPES.SUCCESS)
      trackEvents.userAction('scanned_app_quick_added', { name: scannedApp.name })
    } catch (error) {
      console.error('Failed to quick-add app:', error)
      showMessage('Failed to add', APP_CONSTANTS.MESSAGE_TYPES.ERROR)
    }
  }

  const addAllScannedApps = async () => {
    if (scannedApps.value.length === 0) return

    try {
      isSaving.value = true
      const appsToAdd = scannedApps.value.map(createAppFromScanned)

      apps.value.push(...appsToAdd)
      await AppService.saveApps(apps.value, null)
      await loadApps()

      showMessage(`Added ${appsToAdd.length} app(s)`, APP_CONSTANTS.MESSAGE_TYPES.SUCCESS)
      trackEvents.userAction('scanned_apps_batch_added', { count: appsToAdd.length })

      scannedApps.value = []
      showScanResult.value = false
    } catch (error) {
      console.error('Failed to batch-add apps:', error)
      showMessage('Batch add failed', APP_CONSTANTS.MESSAGE_TYPES.ERROR)
    } finally {
      isSaving.value = false
    }
  }

  const closeScanResult = () => {
    showScanResult.value = false
    scannedApps.value = []
    scannedAppsSearchQuery.value = ''
    showGamesOnly.value = false
    selectedAppType.value = 'all'
  }

  // Get statistics for each category
  const scanResultStats = computed(() => ({
    all: scannedApps.value.length,
    games: scannedApps.value.filter((app) => app['is-game'] === true).length,
    executable: scannedApps.value.filter((app) => app['app-type'] === 'executable').length,
    shortcut: scannedApps.value.filter((app) => app['app-type'] === 'shortcut').length,
    batch: scannedApps.value.filter((app) => app['app-type'] === 'batch').length,
    command: scannedApps.value.filter((app) => app['app-type'] === 'command').length,
    url: scannedApps.value.filter((app) => app['app-type'] === 'url').length,
    steam: scannedApps.value.filter((app) => app['app-type'] === 'steam').length,
    epic: scannedApps.value.filter((app) => app['app-type'] === 'epic').length,
    gog: scannedApps.value.filter((app) => app['app-type'] === 'gog').length,
  }))

  // Filter scan results
  const filteredScannedApps = computed(() => {
    let filtered = scannedApps.value

    // Filter by app type first
    if (selectedAppType.value !== 'all') {
      filtered = filtered.filter((app) => app['app-type'] === selectedAppType.value)
    }

    // Then filter to games only
    if (showGamesOnly.value) {
      filtered = filtered.filter((app) => app['is-game'] === true)
    }

    // Finally filter by search keyword
    if (scannedAppsSearchQuery.value) {
      const query = scannedAppsSearchQuery.value.toLowerCase()
      filtered = filtered.filter((app) => {
        const name = (app.name || '').toLowerCase()
        const cmd = (app.cmd || '').toLowerCase()
        const sourcePath = (app.source_path || '').toLowerCase()
        return name.includes(query) || cmd.includes(query) || sourcePath.includes(query)
      })
    }

    return filtered
  })

  const removeScannedApp = (index) => {
    scannedApps.value.splice(index, 1)
    if (scannedApps.value.length === 0) {
      showScanResult.value = false
    }
  }

  const searchCoverForScannedApp = async (index) => {
    const app = scannedApps.value[index]
    if (!app) return

    try {
      showMessage(`Searching for cover: ${app.name}`, APP_CONSTANTS.MESSAGE_TYPES.INFO)
      const imagePath = await searchCoverImage(app.name)

      if (imagePath) {
        scannedApps.value[index] = { ...app, 'image-path': imagePath }
        showMessage(`Cover found: ${app.name}`, APP_CONSTANTS.MESSAGE_TYPES.SUCCESS)
      } else {
        showMessage(`No cover found: ${app.name}`, APP_CONSTANTS.MESSAGE_TYPES.WARNING)
      }
    } catch (error) {
      console.error('Failed to search cover:', error)
      showMessage('Failed to search cover', APP_CONSTANTS.MESSAGE_TYPES.ERROR)
    }
  }

  const handleCopySuccess = () => showMessage('Copied successfully', APP_CONSTANTS.MESSAGE_TYPES.SUCCESS)
  const handleCopyError = () => showMessage('Copy failed', APP_CONSTANTS.MESSAGE_TYPES.ERROR)

  return {
    // State
    apps,
    filteredApps,
    searchQuery,
    editingApp,
    platform,
    isSaving,
    isDragging,
    viewMode,
    message,
    messageType,
    envVars,
    debouncedSearch,
    isScanning,
    scannedApps,
    showScanResult,
    scannedAppsSearchQuery,
    showGamesOnly,
    selectedAppType,
    // Computed
    messageClass,
    filteredScannedApps,
    scanResultStats,
    // Methods
    init,
    loadApps,
    loadPlatform,
    performSearch,
    clearSearch,
    getOriginalIndex,
    newApp,
    editApp,
    closeAppEditor,
    handleSaveApp,
    showDeleteForm,
    deleteApp,
    cancelDeleteApp,
    confirmDeleteApp,
    deleteConfirmIndex,
    save,
    hasUnsavedChanges,
    onDragStart,
    onDragEnd,
    scanDirectory,
    scanGameLibraries,
    addScannedApp,
    quickAddScannedApp,
    addAllScannedApps,
    closeScanResult,
    removeScannedApp,
    getScannedAppImage,
    searchCoverForScannedApp,
    isTauriEnv,
    showMessage,
    getMessageIcon,
    handleCopySuccess,
    handleCopyError,
  }
}
