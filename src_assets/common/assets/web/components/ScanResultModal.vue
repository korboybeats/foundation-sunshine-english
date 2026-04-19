<template>
  <Transition name="fade">
    <div v-if="show" class="scan-result-overlay" @click.self="$emit('close')">
      <div class="scan-result-modal">
        <!-- Header -->
        <div class="scan-result-header">
          <h5>
            <i class="fas fa-search me-2"></i>{{ t('apps.scan_result_title') }}
            <span class="badge bg-primary ms-2">{{ apps.length }}</span>
            <span v-if="stats.games > 0" class="badge bg-warning text-dark ms-2">
              <i class="fas fa-gamepad me-1"></i>{{ stats.games }}
            </span>
            <span v-if="hasActiveFilter" class="badge bg-info ms-2"> {{ t('apps.scan_result_matched', { count: filteredApps.length }) }} </span>
          </h5>
          <button class="btn-close" :aria-label="t('close')" @click="$emit('close')"></button>
        </div>

        <!-- Search box and filters -->
        <div v-if="apps.length > 0" class="scan-result-search">
          <div class="search-box">
            <i class="fas fa-search search-icon"></i>
            <input
              type="text"
              class="form-control search-input"
              :placeholder="t('apps.scan_result_search_placeholder')"
              v-model="searchQuery"
            />
            <button v-if="searchQuery" class="btn-clear-search" @click="searchQuery = ''" type="button">
              <i class="fas fa-times"></i>
            </button>
          </div>

          <!-- Filter button group -->
          <div class="scan-result-filters mt-2">
            <div class="d-flex flex-wrap gap-2 align-items-center">
              <!-- App-type filter -->
              <div class="btn-group btn-group-sm flex-wrap" role="group">
                <button
                  class="btn"
                  :class="selectedType === 'all' ? 'btn-primary' : 'btn-outline-primary'"
                  @click="selectedType = 'all'"
                  type="button"
                >
                  {{ t('apps.scan_result_filter_all') }}
                  <span class="badge bg-dark ms-1">{{ stats.all }}</span>
                </button>
                <button
                  v-if="stats.shortcut > 0"
                  class="btn"
                  :class="selectedType === 'shortcut' ? 'btn-info' : 'btn-outline-info'"
                  @click="selectedType = 'shortcut'"
                  type="button"
                  :title="t('apps.scan_result_filter_shortcut_title')"
                >
                  <i class="fas fa-link me-1"></i>{{ t('apps.scan_result_filter_shortcut') }}
                  <span class="badge bg-dark ms-1">{{ stats.shortcut }}</span>
                </button>
                <button
                  v-if="stats.executable > 0"
                  class="btn"
                  :class="selectedType === 'executable' ? 'btn-primary' : 'btn-outline-primary'"
                  @click="selectedType = 'executable'"
                  type="button"
                  :title="t('apps.scan_result_filter_executable_title')"
                >
                  <i class="fas fa-file-code me-1"></i>{{ t('apps.scan_result_filter_executable') }}
                  <span class="badge bg-dark ms-1">{{ stats.executable }}</span>
                </button>
                <button
                  v-if="stats.batch > 0 || stats.command > 0"
                  class="btn"
                  :class="
                    selectedType === 'batch' || selectedType === 'command' ? 'btn-secondary' : 'btn-outline-secondary'
                  "
                  @click="selectedType = stats.batch > 0 ? 'batch' : 'command'"
                  type="button"
                  :title="t('apps.scan_result_filter_script_title')"
                >
                  <i class="fas fa-terminal me-1"></i>{{ t('apps.scan_result_filter_script') }}
                  <span class="badge bg-dark ms-1">{{ stats.batch + stats.command }}</span>
                </button>
                <button
                  v-if="stats.url > 0"
                  class="btn"
                  :class="selectedType === 'url' ? 'btn-success' : 'btn-outline-success'"
                  @click="selectedType = 'url'"
                  type="button"
                  :title="t('apps.scan_result_filter_url_title')"
                >
                  <i class="fas fa-globe me-1"></i>{{ t('apps.scan_result_filter_url') }}
                  <span class="badge bg-dark ms-1">{{ stats.url }}</span>
                </button>
                <button
                  v-if="stats.steam > 0"
                  class="btn"
                  :class="selectedType === 'steam' ? 'btn-primary' : 'btn-outline-primary'"
                  @click="selectedType = 'steam'"
                  type="button"
                  :title="t('apps.scan_result_filter_steam_title')"
                >
                  <i class="fab fa-steam me-1"></i>Steam
                  <span class="badge bg-dark ms-1">{{ stats.steam }}</span>
                </button>
                <button
                  v-if="stats.epic > 0"
                  class="btn"
                  :class="selectedType === 'epic' ? 'btn-dark' : 'btn-outline-dark'"
                  @click="selectedType = 'epic'"
                  type="button"
                  :title="t('apps.scan_result_filter_epic_title')"
                >
                  <i class="fas fa-store me-1"></i>Epic
                  <span class="badge bg-dark ms-1">{{ stats.epic }}</span>
                </button>
                <button
                  v-if="stats.gog > 0"
                  class="btn"
                  :class="selectedType === 'gog' ? 'btn-secondary' : 'btn-outline-secondary'"
                  @click="selectedType = 'gog'"
                  type="button"
                  :title="t('apps.scan_result_filter_gog_title')"
                >
                  <i class="fas fa-compact-disc me-1"></i>GOG
                  <span class="badge bg-dark ms-1">{{ stats.gog }}</span>
                </button>
              </div>

              <!-- Games-only filter -->
              <button
                v-if="stats.games > 0"
                class="btn btn-sm"
                :class="gamesOnly ? 'btn-warning' : 'btn-outline-warning'"
                @click="gamesOnly = !gamesOnly"
                type="button"
              >
                <i class="fas fa-gamepad me-1"></i>
                {{ gamesOnly ? t('apps.scan_result_show_all') : t('apps.scan_result_games_only') }}
                <span class="badge bg-dark ms-1">{{ stats.games }}</span>
              </button>
            </div>
          </div>
        </div>

        <!-- App list -->
        <div class="scan-result-body">
          <div v-if="apps.length === 0" class="text-center text-muted py-4">
            <i class="fas fa-folder-open fa-3x mb-3"></i>
            <p>{{ t('apps.scan_result_no_apps') }}</p>
          </div>
          <div v-else-if="filteredApps.length === 0" class="text-center text-muted py-4">
            <i class="fas fa-search fa-3x mb-3"></i>
            <p>{{ t('apps.scan_result_no_matches') }}</p>
            <p class="small">{{ t('apps.scan_result_try_different_keywords') }}</p>
          </div>
          <div v-else class="scan-result-list">
            <div v-for="app in filteredApps" :key="app.source_path" class="scan-result-item">
              <!-- App icon -->
              <div class="scan-app-icon">
                <img
                  v-if="app['image-path']"
                  :src="app['image-path']"
                  :alt="app.name"
                  @error="$event.target.style.display = 'none'"
                />
                <svg v-else width="80" height="80" viewBox="0 0 100 100" xmlns="http://www.w3.org/2000/svg">
                  <rect width="100" height="100" fill="#667eea" />
                  <text
                    x="50"
                    y="50"
                    font-size="40"
                    font-weight="bold"
                    fill="#fff"
                    text-anchor="middle"
                    dominant-baseline="central"
                  >
                    {{ app.name.charAt(0).toUpperCase() }}
                  </text>
                </svg>
              </div>

              <!-- App info -->
              <div class="scan-app-info">
                <div class="scan-app-name">
                  <i v-if="app['is-game']" class="fas fa-gamepad me-1 text-warning" :title="t('apps.scan_result_game')"></i>
                  {{ app.name }}
                  <span v-if="app['app-type']" class="badge ms-2" :class="getAppTypeBadgeClass(app['app-type'])">
                    {{ getAppTypeLabel(app['app-type']) }}
                  </span>
                  <span v-if="app['is-game']" class="badge bg-warning text-dark ms-2">
                    <i class="fas fa-gamepad me-1"></i>{{ t('apps.scan_result_game') }}
                  </span>
                </div>
                <div class="scan-app-cmd small">{{ app.cmd }}</div>
                <div class="scan-app-path small"><i class="fas fa-folder-open me-1"></i>{{ app.source_path }}</div>
              </div>

              <!-- Action buttons -->
              <div class="scan-app-actions">
                <button class="btn btn-sm btn-outline-primary" @click="$emit('edit', app)" :title="t('apps.scan_result_edit_title')">
                  <i class="fas fa-edit"></i>
                </button>
                <button
                  class="btn btn-sm btn-outline-success"
                  @click="$emit('quick-add', app, apps.indexOf(app))"
                  :title="t('apps.scan_result_quick_add_title')"
                >
                  <i class="fas fa-plus"></i>
                </button>
                <button
                  class="btn btn-sm btn-outline-danger"
                  @click="$emit('remove', apps.indexOf(app))"
                  :title="t('apps.scan_result_remove_title')"
                >
                  <i class="fas fa-times"></i>
                </button>
              </div>
            </div>
          </div>
        </div>

        <!-- Footer action bar -->
        <div v-if="apps.length > 0" class="scan-result-footer">
          <button class="btn btn-secondary" @click="$emit('close')"><i class="fas fa-times me-1"></i>{{ t('_common.cancel') }}</button>
          <button class="btn btn-primary" @click="$emit('add-all')" :disabled="saving">
            <i class="fas" :class="saving ? 'fa-spinner fa-spin' : 'fa-check-double'"></i>
            <span class="ms-1">{{ t('apps.scan_result_add_all') }}</span>
          </button>
        </div>
      </div>
    </div>
  </Transition>
</template>

<script setup>
import { ref, computed, watch } from 'vue'
import { useI18n } from 'vue-i18n'

const { t } = useI18n()

const props = defineProps({
  show: {
    type: Boolean,
    default: false,
  },
  apps: {
    type: Array,
    default: () => [],
  },
  saving: {
    type: Boolean,
    default: false,
  },
})

defineEmits(['close', 'edit', 'quick-add', 'remove', 'add-all'])

// Local state
const searchQuery = ref('')
const selectedType = ref('all')
const gamesOnly = ref(false)

// Reset filters
watch(
  () => props.show,
  (newVal) => {
    if (!newVal) {
      searchQuery.value = ''
      selectedType.value = 'all'
      gamesOnly.value = false
    }
  }
)

// When the apps reference is fully replaced (new scan result), reset all filter state
watch(
  () => props.apps,
  (newApps) => {
    const hasGames = newApps.length > 0 && newApps.some((app) => app['is-game'] === true)
    gamesOnly.value = hasGames
    selectedType.value = 'all'
    searchQuery.value = ''
  }
)

// When the array mutates internally (quick-add/remove via splice), do only defensive corrections
watch(
  () => [props.apps.length, ...props.apps.map((a) => a['app-type'])],
  () => {
    // If the currently selected type no longer has matching entries, fall back to 'all'
    if (selectedType.value !== 'all') {
      const hasType = props.apps.some((app) => app['app-type'] === selectedType.value)
      if (!hasType) {
        selectedType.value = 'all'
      }
    }
    // If the games-only filter is on but no game entries remain, turn it off
    if (gamesOnly.value && !props.apps.some((app) => app['is-game'] === true)) {
      gamesOnly.value = false
    }
  }
)

// Statistics
const stats = computed(() => ({
  all: props.apps.length,
  games: props.apps.filter((app) => app['is-game'] === true).length,
  executable: props.apps.filter((app) => app['app-type'] === 'executable').length,
  shortcut: props.apps.filter((app) => app['app-type'] === 'shortcut').length,
  batch: props.apps.filter((app) => app['app-type'] === 'batch').length,
  command: props.apps.filter((app) => app['app-type'] === 'command').length,
  url: props.apps.filter((app) => app['app-type'] === 'url').length,
  steam: props.apps.filter((app) => app['app-type'] === 'steam').length,
  epic: props.apps.filter((app) => app['app-type'] === 'epic').length,
  gog: props.apps.filter((app) => app['app-type'] === 'gog').length,
}))

// Whether any filters are active
const hasActiveFilter = computed(() => searchQuery.value || gamesOnly.value || selectedType.value !== 'all')

// Filtered app list
const filteredApps = computed(() => {
  let filtered = props.apps

  // Filter by app type
  if (selectedType.value !== 'all') {
    filtered = filtered.filter((app) => app['app-type'] === selectedType.value)
  }

  // Filter to games only
  if (gamesOnly.value) {
    filtered = filtered.filter((app) => app['is-game'] === true)
  }

  // Filter by search keyword
  if (searchQuery.value) {
    const query = searchQuery.value.toLowerCase()
    filtered = filtered.filter((app) => {
      const name = (app.name || '').toLowerCase()
      const cmd = (app.cmd || '').toLowerCase()
      const sourcePath = (app.source_path || '').toLowerCase()
      return name.includes(query) || cmd.includes(query) || sourcePath.includes(query)
    })
  }

  return filtered
})

// App-type label
const getAppTypeLabel = (appType) => {
  const typeMap = {
    executable: t('apps.scan_result_type_executable'),
    shortcut: t('apps.scan_result_type_shortcut'),
    batch: t('apps.scan_result_type_batch'),
    command: t('apps.scan_result_type_command'),
    url: t('apps.scan_result_type_url'),
    steam: 'Steam',
    epic: 'Epic Games',
    gog: 'GOG',
  }
  return typeMap[appType] || appType
}

// App-type badge styles
const getAppTypeBadgeClass = (appType) => {
  const classMap = {
    executable: 'bg-primary',
    shortcut: 'bg-info',
    batch: 'bg-warning text-dark',
    command: 'bg-warning text-dark',
    url: 'bg-success',
    steam: 'bg-primary',
    epic: 'bg-dark',
    gog: 'bg-secondary',
  }
  return classMap[appType] || 'bg-secondary'
}
</script>
