<template>
  <div>
    <Navbar v-if="!showSetupWizard" />

    <!-- First-time setup wizard -->
    <SetupWizard
      v-if="showSetupWizard"
      :adapters="adapters"
      :display-devices="displayDevices"
      :has-locale="hasLocale"
      @setup-complete="onSetupComplete"
    />

    <!-- Normal home content -->
    <div v-if="!showSetupWizard" id="content" class="container">
      <div class="page-header mt-2 mb-4">
        <h1 class="page-title">
          {{ $t('index.welcome') }}
        </h1>
        <p class="page-subtitle">{{ $t('index.description') }}</p>
      </div>

      <!-- Error logs -->
      <ErrorLogs :fatal-logs="fatalLogs" />

      <!-- Version info -->
      <VersionCard
        :version="version"
        :github-version="githubVersion"
        :pre-release-version="preReleaseVersion"
        :notify-pre-releases="notifyPreReleases"
        :loading="loading"
        :installed-version-not-stable="installedVersionNotStable"
        :stable-build-available="stableBuildAvailable"
        :pre-release-build-available="preReleaseBuildAvailable"
        :build-version-is-dirty="buildVersionIsDirty"
        :parsed-stable-body="parsedStableBody"
        :parsed-pre-release-body="parsedPreReleaseBody"
      />

      <!-- Resource card -->
      <div class="my-4">
        <ResourceCard />
      </div>
    </div>
  </div>
</template>

<script setup>
import { onMounted } from 'vue'
import Navbar from '../components/layout/Navbar.vue'
import SetupWizard from '../components/SetupWizard.vue'
import ResourceCard from '../components/common/ResourceCard.vue'
import ErrorLogs from '../components/common/ErrorLogs.vue'
import VersionCard from '../components/common/VersionCard.vue'
import { useVersion } from '../composables/useVersion.js'
import { useLogs } from '../composables/useLogs.js'
import { useSetupWizard } from '../composables/useSetupWizard.js'
import { trackEvents } from '../config/firebase.js'

// Use the composables
const {
  version,
  githubVersion,
  preReleaseVersion,
  notifyPreReleases,
  loading,
  installedVersionNotStable,
  stableBuildAvailable,
  preReleaseBuildAvailable,
  buildVersionIsDirty,
  parsedStableBody,
  parsedPreReleaseBody,
  fetchVersions,
} = useVersion()

const { fatalLogs, fetchLogs } = useLogs()

const { showSetupWizard, adapters, displayDevices, hasLocale, checkSetupWizard, onSetupComplete } = useSetupWizard()

// Report GPU info
const reportGPUInfo = (config) => {
  try {
    const adapters = config.adapters || []
    const adapterNames = adapters.map((a) => (typeof a === 'string' ? a : a?.name || String(a))).join(', ')

    const gpuInfo = {
      platform: config.platform || 'unknown',
      adapter_count: adapters.length,
      adapters: adapterNames,
      selected_adapter: config.adapter_name || (adapters.length ? 'auto' : 'none'),
      has_selected_adapter: !!config.adapter_name,
    }

    trackEvents.gpuReported(gpuInfo)
  } catch (error) {
    console.error('Failed to report GPU info:', error)
  }
}

// Initialization
onMounted(async () => {
  // Record page view
  trackEvents.pageView('home')

  try {
    const config = await fetch('/api/config').then((r) => r.json())

    setTimeout(() => {
      reportGPUInfo(config)
    }, 1000)

    // Check whether the setup wizard needs to be shown
    if (checkSetupWizard(config)) {
      return
    }

    // Fetch version info
    await fetchVersions(config)

    // Fetch logs
    await fetchLogs()

    // Update the page title
    if (version.value) {
      document.title += ` Ver ${version.value.version}`
    }
  } catch (e) {
    // In preview mode the API may not be available, so only log a warning
    if (e?.message?.includes('JSON') || e?.message?.includes('<!DOCTYPE')) {
      console.warn('API not available in preview mode:', e.message)
    } else {
      console.error('Failed to initialize:', e)
      trackEvents.errorOccurred('home_initialization', e.message)
    }
  }
})
</script>

<style>
@import '../styles/global.less';
</style>
