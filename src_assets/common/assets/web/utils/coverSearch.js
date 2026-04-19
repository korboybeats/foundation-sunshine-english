/**
 * Cover search utility module
 * Provides unified IGDB and Steam cover search functionality
 */

import { searchSteamCovers } from './steamApi.js'

// Shared caches (module-level, avoids repeated creation)
const bucketCache = new Map()
const gameCache = new Map()

// IGDB-related constants
const IGDB_BASE_URL = 'https://lizardbyte.github.io/GameDB'
const IGDB_IMAGE_URL = 'https://images.igdb.com/igdb/image/upload/t_cover_big_2x'

// Cached Tauri environment detection result
let _isTauriEnv = null

/**
 * Detect whether running inside the Tauri environment
 * @returns {boolean} Whether running in Tauri
 */
function isTauriEnv() {
  if (_isTauriEnv === null) {
    _isTauriEnv = typeof window !== 'undefined' && !!(window.isTauri || window.__TAURI__)
  }
  return _isTauriEnv
}

/**
 * Build a proxy URL (used to bypass CORS restrictions)
 * @param {string} url Original URL
 * @returns {string} Proxy URL or the original URL
 */
function buildProxyUrl(url) {
  return isTauriEnv() ? `/_proxy/?url=${encodeURIComponent(url)}` : url
}

/**
 * Get the search bucket (used for IGDB search)
 * Note: IGDB buckets only support ASCII letters and digits; non-ASCII characters such as CJK return '@'
 * @param {string} name Application name
 * @returns {string} Bucket identifier
 */
export function getSearchBucket(name) {
  const bucket = name
    .substring(0, 2)
    .toLowerCase()
    .replace(/[^a-z\d]/g, '')
  return bucket || '@'
}

/**
 * Check whether the search term is suitable for IGDB search
 * The IGDB bucket system only supports ASCII letters; non-ASCII characters such as CJK cannot be matched correctly
 * @param {string} name Search name
 * @returns {boolean} Whether suitable for IGDB search
 */
function isValidForIGDB(name) {
  if (!name) return false
  // Check whether it contains at least one ASCII letter or digit
  return /[a-zA-Z\d]/.test(name)
}

// Pre-compiled regular expressions
const SEPARATOR_REGEX = /[:\-_''""]/g
const WHITESPACE_REGEX = /\s+/g

/**
 * Normalize the search string
 * @param {string} str Original string
 * @returns {string} Normalized string
 */
function normalizeSearchString(str) {
  return str.toLowerCase().replace(SEPARATOR_REGEX, ' ').replace(WHITESPACE_REGEX, ' ').trim()
}

/**
 * Generate the bigrams set for a string
 * @param {string} str Input string
 * @returns {Set<string>} Set of bigrams
 */
function getBigrams(str) {
  const bigrams = new Set()
  const len = str.length - 1
  for (let i = 0; i < len; i++) {
    bigrams.add(str.substring(i, i + 2))
  }
  return bigrams
}

/**
 * Calculate string similarity (Dice coefficient)
 * @param {string} str1 String 1
 * @param {string} str2 String 2
 * @returns {number} Similarity 0-1
 */
function calculateSimilarity(str1, str2) {
  const s1 = normalizeSearchString(str1)
  const s2 = normalizeSearchString(str2)

  if (s1 === s2) return 1
  if (s1.length < 2 || s2.length < 2) return 0

  const bigrams1 = getBigrams(s1)
  const bigrams2 = getBigrams(s2)

  let intersection = 0
  for (const bigram of bigrams1) {
    if (bigrams2.has(bigram)) intersection++
  }

  return (2 * intersection) / (bigrams1.size + bigrams2.size)
}

/**
 * Check whether a name matches the search term
 * @param {string} gameName Game name
 * @param {string} searchTerm Search term
 * @returns {{match: boolean, score: number}} Match result and score
 */
function matchesSearch(gameName, searchTerm) {
  const normalizedGame = normalizeSearchString(gameName)
  const normalizedSearch = normalizeSearchString(searchTerm)

  // Exact match
  if (normalizedGame === normalizedSearch) {
    return { match: true, score: 1 }
  }

  // Prefix match (high priority)
  if (normalizedGame.startsWith(normalizedSearch)) {
    return { match: true, score: 0.95 }
  }

  // Substring match
  if (normalizedGame.includes(normalizedSearch)) {
    return { match: true, score: 0.85 }
  }

  // Word match (all words in the search term appear in the game name)
  const searchWords = normalizedSearch.split(' ').filter((w) => w.length > 1)
  if (searchWords.length > 0) {
    const gameWords = normalizedGame.split(' ')
    const allWordsMatch = searchWords.every((sw) => gameWords.some((gw) => gw.startsWith(sw) || gw.includes(sw)))
    if (allWordsMatch) {
      return { match: true, score: 0.8 }
    }
  }

  // Similarity match
  const similarity = calculateSimilarity(gameName, searchTerm)
  if (similarity > 0.5) {
    return { match: true, score: similarity * 0.7 }
  }

  return { match: false, score: 0 }
}

/**
 * Cached fetch helper
 * @param {Map} cache Cache Map
 * @param {string} key Cache key
 * @param {Function} fetchFn Function that fetches the data
 * @returns {Promise<any>} Data
 */
async function fetchWithCache(cache, key, fetchFn) {
  const cached = cache.get(key)
  if (cached !== undefined) return cached
  const data = await fetchFn()
  cache.set(key, data)
  return data
}

/**
 * Extract the hash from a cover URL and build the full URL
 * @param {string} thumbUrl Thumbnail URL
 * @param {string} size Image size
 * @param {string} ext File extension
 * @returns {string} Full image URL
 */
function buildIGDBImageUrl(thumbUrl, size = 't_cover_big_2x', ext = 'png') {
  const lastSlash = thumbUrl.lastIndexOf('/')
  const lastDot = thumbUrl.lastIndexOf('.')
  const hash = thumbUrl.substring(lastSlash + 1, lastDot)
  return `https://images.igdb.com/igdb/image/upload/${size}/${hash}.${ext}`
}

/**
 * Search IGDB cover (single result, returns URL string)
 * @param {string} searchName Search name
 * @param {string} bucket Bucket identifier
 * @returns {Promise<string>} Cover URL, or empty string if not found
 */
export async function searchIGDBCover(searchName, bucket) {
  // Check whether the search term is suitable for IGDB search
  if (!isValidForIGDB(searchName)) {
    return ''
  }

  try {
    const maps = await fetchWithCache(bucketCache, bucket, async () => {
      const url = `${IGDB_BASE_URL}/buckets/${bucket}.json`
      const response = await fetch(buildProxyUrl(url))
      return response.ok ? response.json() : null
    })

    if (!maps) return ''

    let bestMatch = null
    let bestScore = 0

    const ids = Object.keys(maps)
    for (let i = 0; i < ids.length; i++) {
      const id = ids[i]
      const { match, score } = matchesSearch(maps[id].name, searchName)
      if (match && score > bestScore) {
        bestScore = score
        bestMatch = id
      }
    }

    if (!bestMatch) return ''

    const game = await fetchWithCache(gameCache, bestMatch, async () => {
      const url = `${IGDB_BASE_URL}/games/${bestMatch}.json`
      const res = await fetch(buildProxyUrl(url))
      return res.ok ? res.json() : null
    })

    if (!game?.cover?.url) return ''

    return buildIGDBImageUrl(game.cover.url)
  } catch (error) {
    console.warn(`Failed to search IGDB cover: ${searchName}`, error)
    return ''
  }
}

/**
 * Search IGDB covers (multiple results, returns array)
 * @param {string} name Application name
 * @param {AbortSignal} signal Optional AbortSignal to cancel the request
 * @param {number} maxResults Maximum number of results
 * @returns {Promise<Array>} Cover result array
 */
export async function searchIGDBCovers(name, signal = null, maxResults = 20) {
  if (!name) return []

  // Check whether the search term is suitable for IGDB search (IGDB only supports ASCII search)
  if (!isValidForIGDB(name)) {
    console.debug(`IGDB search skipped: search term "${name}" contains no ASCII characters`)
    return []
  }

  const bucket = getSearchBucket(name)

  try {
    const maps = await fetchWithCache(bucketCache, bucket, async () => {
      const url = `${IGDB_BASE_URL}/buckets/${bucket}.json`
      const response = await fetch(buildProxyUrl(url), { signal })
      if (!response.ok) {
        // 404 means the bucket does not exist, which is normal — return an empty object
        if (response.status === 404) {
          return {}
        }
        throw new Error('Failed to search covers')
      }
      return response.json()
    })

    // Use the improved matching algorithm; collect all matches and sort by score
    const matches = []
    const ids = Object.keys(maps)
    for (let i = 0; i < ids.length; i++) {
      const id = ids[i]
      const { match, score } = matchesSearch(maps[id].name, name)
      if (match) {
        matches.push({ id, score, name: maps[id].name })
      }
    }

    // Sort by score descending, take the top maxResults
    matches.sort((a, b) => b.score - a.score)
    const matchedIds = matches.slice(0, maxResults).map((m) => m.id)

    // Fetch game details in parallel, using the cache
    const games = await Promise.all(
      matchedIds.map(async (id) => {
        return fetchWithCache(gameCache, id, async () => {
          try {
            const url = `${IGDB_BASE_URL}/games/${id}.json`
            const res = await fetch(buildProxyUrl(url), { signal })
            return res.json()
          } catch {
            return null
          }
        })
      })
    )

    const results = []
    for (let i = 0; i < games.length; i++) {
      const game = games[i]
      if (game?.cover?.url) {
        const thumb = game.cover.url
        results.push({
          name: game.name,
          key: `igdb_${game.id}`,
          source: 'igdb',
          url: buildIGDBImageUrl(thumb, 't_cover_big', 'jpg'),
          saveUrl: buildIGDBImageUrl(thumb),
        })
      }
    }
    return results
  } catch (error) {
    if (error.name === 'AbortError') {
      throw error
    }
    console.error('Failed to search IGDB covers:', error)
    return []
  }
}

/**
 * Search for cover image (single result, used by useApps.js)
 * Searches IGDB and Steam at the same time and returns the first one found
 * @param {string} appName Application name
 * @returns {Promise<string>} Cover URL, or empty string if not found
 */
export async function searchCoverImage(appName) {
  if (!appName) return ''

  const bucket = getSearchBucket(appName)

  try {
    const [igdbResult, steamResult] = await Promise.allSettled([
      searchIGDBCover(appName, bucket),
      searchSteamCovers(appName, 1).then((results) => results[0]?.saveUrl || ''),
    ])

    return (
      (igdbResult.status === 'fulfilled' && igdbResult.value) ||
      (steamResult.status === 'fulfilled' && steamResult.value) ||
      ''
    )
  } catch (error) {
    console.warn(`Failed to search cover: ${appName}`, error)
    return ''
  }
}

/**
 * Batch search cover images
 * @param {Array} appList Application list
 * @returns {Promise<Array>} Application list with cover URLs
 */
export async function batchSearchCoverImages(appList) {
  const results = await Promise.allSettled(
    appList.map(async (app) => ({
      ...app,
      'image-path': await searchCoverImage(encodeURIComponent(app.name)),
    }))
  )
  return results.map((result, index) => (result.status === 'fulfilled' ? result.value : appList[index]))
}

/**
 * Search both IGDB and Steam covers (multiple results, used by CoverFinder.vue)
 * @param {string} name Application name
 * @param {AbortSignal} signal Optional AbortSignal to cancel the request
 * @returns {Promise<{igdb: Array, steam: Array}>} Object containing IGDB and Steam results
 */
export async function searchAllCovers(name, signal = null) {
  if (!name) {
    return { igdb: [], steam: [] }
  }

  try {
    const [igdbResults, steamResults] = await Promise.allSettled([
      searchIGDBCovers(name, signal),
      searchSteamCovers(name),
    ])

    return {
      igdb: igdbResults.status === 'fulfilled' ? igdbResults.value : [],
      steam: steamResults.status === 'fulfilled' ? steamResults.value : [],
    }
  } catch (error) {
    if (error.name === 'AbortError') {
      throw error
    }
    console.error('Failed to search covers:', error)
    return { igdb: [], steam: [] }
  }
}

/**
 * Clear caches
 */
export function clearCache() {
  bucketCache.clear()
  gameCache.clear()
}
