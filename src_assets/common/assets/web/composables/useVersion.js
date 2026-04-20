import { ref, computed } from 'vue'
import { marked } from 'marked'
import SunshineVersion from '../sunshine_version.js'
import { trackEvents } from '../config/firebase.js'

const GITHUB_API_BASE = 'https://api.github.com/repos/korboybeats/foundation-sunshine-english/releases'

/**
 * Parse Markdown content
 */
const parseMarkdown = (text) => {
  if (!text) return ''
  const normalized = text.replace(/\r\n?/g, '\n')
  return marked(normalized, { breaks: true, gfm: true })
}

/**
 * Safely fetch data from GitHub
 */
const fetchGitHub = async (url) => {
  try {
    const response = await fetch(url)
    if (!response.ok) return null
    return await response.json()
  } catch (e) {
    console.error(`Failed to fetch ${url}:`, e)
    return null
  }
}

/**
 * Convert a config value to a boolean
 */
const toBoolean = (value) => {
  if (typeof value === 'string') {
    return value.toLowerCase() === 'true'
  }
  return Boolean(value)
}

/**
 * Version management composable
 */
export function useVersion() {
  const version = ref(null)
  const githubVersion = ref(null)
  const preReleaseVersion = ref(null)
  const notifyPreReleases = ref(false)
  const loading = ref(true)

  // Computed properties
  const installedVersionNotStable = computed(() => 
    githubVersion.value?.isLessThan?.(version.value) ?? false
  )

  // English Edition: updates are managed by the wrapper's weekly scheduled
  // task which reads HKLM\SOFTWARE\SunshineEnglishEdition\Version. The Web UI's
  // own version-check compares sunshine.exe's FileVersion (e.g.
  // "0.0.0.<commit>") against our release tag (e.g. "v2026.04.19-english")
  // which always parses as "update available" because 2026 > 0. Disable the
  // stable/pre-release banners here so they stop screaming at users who are
  // already on the latest version via the wrapper.
  const stableBuildAvailable = computed(() => false)
  const preReleaseBuildAvailable = computed(() => false)

  const buildVersionIsDirty = computed(() => {
    const v = version.value?.version
    if (!v) return false
    const parts = v.split('.')
    return parts.length === 5 && v.includes('dirty')
  })

  const parsedStableBody = computed(() => 
    parseMarkdown(githubVersion.value?.release?.body)
  )
  
  const parsedPreReleaseBody = computed(() => 
    parseMarkdown(preReleaseVersion.value?.release?.body)
  )

  /**
   * Fetch version information
   */
  const fetchVersions = async (config) => {
    loading.value = true
    
    try {
      notifyPreReleases.value = toBoolean(config.notify_pre_releases)
      version.value = new SunshineVersion(null, config.version)
      
      // Fetch GitHub version info in parallel
      const [latestData, releases] = await Promise.all([
        fetchGitHub(`${GITHUB_API_BASE}/latest`),
        fetchGitHub(GITHUB_API_BASE)
      ])

      if (latestData) {
        githubVersion.value = new SunshineVersion(latestData, null)
      }

      if (Array.isArray(releases)) {
        const preRelease = releases.find((r) => r.prerelease)
        if (preRelease) {
          preReleaseVersion.value = new SunshineVersion(preRelease, null)
        }
      }

      // Record the version-check event
      if (githubVersion.value && version.value) {
        trackEvents.versionChecked(version.value.version, githubVersion.value.version)
      }
    } catch (e) {
      console.error('Version check failed:', e)
      trackEvents.errorOccurred('version_check', e.message)
    } finally {
      loading.value = false
    }
  }

  return {
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
  }
}
