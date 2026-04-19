/**
 * Steam API utility module
 * Provides Steam app search and cover fetching
 *
 */

// Steam CDN base URL
const STEAM_CDN_BASE = 'https://cdn.cloudflare.steamstatic.com/steam/apps'

// SteamGridDB API base URL
const STEAMGRIDDB_API_BASE = '/steamgriddb'

// Cover URL cache
const coverUrlCache = new Map()

// SteamGridDB cache
const steamGridDBCache = new Map()

// Image existence cache
const imageExistsCache = new Map()

/**
 * Build URL query string
 * @param {Object} options Parameter object
 * @returns {string} Query string
 */
function buildQueryString(options) {
  const params = new URLSearchParams()
  Object.entries(options).forEach(([key, value]) => {
    if (value !== undefined && value !== null) {
      params.append(key, value)
    }
  })
  const str = params.toString()
  return str ? `?${str}` : ''
}

/**
 * Generic fetch wrapper
 * @param {string} url Request URL
 * @param {Object} options Fetch options
 * @returns {Promise<Object|null>} Response data
 */
async function fetchJson(url, options = {}) {
  try {
    const response = await fetch(url, options)
    if (!response.ok) {
      return null
    }
    return await response.json()
  } catch {
    return null
  }
}

/**
 * Search Steam apps (uses the Steam Store search API)
 * @param {string} searchName Search name
 * @param {number} maxResults Maximum number of results
 * @returns {Promise<Array>} List of matching Steam apps
 */
export async function searchSteamApps(searchName, maxResults = 20) {
  if (!searchName?.trim()) {
    return []
  }

  const data = await fetchJson(`/steam-store/api/storesearch/?term=${encodeURIComponent(searchName)}&l=english&cc=US`)

  if (!data?.items?.length) {
    return []
  }

  return data.items
    .filter((item) => item.type === 'app')
    .slice(0, maxResults)
    .map(({ id, name, tiny_image, platforms, price, metascore }) => ({
      appid: id,
      name,
      tiny_image,
      platforms,
      price,
      metascore,
    }))
}

/**
 * Load Steam app list (deprecated, kept for compatibility)
 * @deprecated Use searchSteamApps instead
 * @returns {Promise<Array>} Empty array
 */
export async function loadSteamApps() {
  console.warn('loadSteamApps is deprecated, please use searchSteamApps to search directly')
  return []
}

/**
 * Get Steam app details
 * @param {number} appId Steam app ID
 * @returns {Promise<Object|null>} Steam app details
 */
export async function getSteamAppDetails(appId) {
  const data = await fetchJson(`/steam-store/api/appdetails?appids=${appId}&l=english`)
  return data?.[appId]?.success ? data[appId].data : null
}

/**
 * Search Steam app covers (fast mode)
 * Optimization: use CDN URLs directly without calling the details API for a large speed boost
 * @param {string} name Application name
 * @param {number} maxResults Maximum number of results
 * @returns {Promise<Array>} Cover list
 */
export async function searchSteamCovers(name, maxResults = 20) {
  if (!name) {
    return []
  }

  const matches = await searchSteamApps(name, maxResults)

  if (!matches.length) {
    return []
  }

  const coverPromises = matches.map(async ({ appid, name: appName }) => ({
    name: appName,
    appid,
    source: 'steam',
    url: getSteamCoverUrl(appid, 'header'),
    saveUrl: await getCachedBestCoverUrl(appid),
    key: `steam_${appid}`,
  }))

  return Promise.all(coverPromises)
}

/**
 * Search Steam app covers (full mode, includes details)
 * @param {string} name Application name
 * @param {number} maxResults Maximum number of results
 * @returns {Promise<Array>} Cover list (with details)
 */
export async function searchSteamCoversWithDetails(name, maxResults = 20) {
  if (!name) {
    return []
  }

  const matches = await searchSteamApps(name, maxResults)

  if (!matches.length) {
    return []
  }

  const detailPromises = matches.map(async ({ appid }) => {
    const gameData = await getSteamAppDetails(appid)

    if (!gameData) {
      return null
    }

    const headerImage = gameData.header_image || gameData.capsule_image || gameData.capsule_imagev5
    const saveUrl = await getCachedBestCoverUrl(appid)

    return {
      name: gameData.name,
      appid,
      source: 'steam',
      url: headerImage,
      saveUrl,
      key: `steam_${appid}`,
      type: gameData.type || 'game',
      shortDescription: gameData.short_description || '',
      developers: gameData.developers || [],
      publishers: gameData.publishers || [],
      releaseDate: gameData.release_date || null,
    }
  })

  const results = await Promise.all(detailPromises)
  return results.filter((item) => item?.url)
}

// Cover type mapping
const COVER_TYPE_MAP = {
  header: 'header.jpg',
  header_292x136: 'header_292x136.jpg',
  capsule: 'capsule_231x87.jpg',
  capsule_231x87: 'capsule_231x87.jpg',
  capsule_616x353: 'capsule_616x353.jpg',
  library: 'library_600x900.jpg',
  library_600x900: 'library_600x900.jpg',
  library_2x: 'library_600x900_2x.jpg',
  library_600x900_2x: 'library_600x900_2x.jpg',
  library_hero: 'library_hero.jpg',
  library_hero_2x: 'library_hero_2x.jpg',
  logo: 'logo.png',
  page_bg: 'page_bg_generated_v6b.jpg',
}

/**
 * Get Steam cover image URL
 * @param {number} appId Steam app ID
 * @param {string} type Cover type
 * @returns {string} Cover image URL
 */
export function getSteamCoverUrl(appId, type = 'header') {
  const filename = COVER_TYPE_MAP[type] || COVER_TYPE_MAP.header
  return `${STEAM_CDN_BASE}/${appId}/${filename}`
}

/**
 * Check whether an image URL is valid (cached)
 * @param {string} url Image URL
 * @returns {Promise<boolean>} Whether it is valid
 */
export async function checkImageExists(url) {
  if (imageExistsCache.has(url)) {
    return imageExistsCache.get(url)
  }

  try {
    const response = await fetch(url, { method: 'HEAD' })
    const exists = response.ok
    imageExistsCache.set(url, exists)
    return exists
  } catch {
    imageExistsCache.set(url, false)
    return false
  }
}

/**
 * Get the best available Steam cover URL (cached)
 * @param {number} appId Steam app ID
 * @returns {Promise<string>} Best cover URL
 */
export async function getCachedBestCoverUrl(appId) {
  if (coverUrlCache.has(appId)) {
    return coverUrlCache.get(appId)
  }

  const libraryUrl = getSteamCoverUrl(appId, 'library')
  const libraryExists = await checkImageExists(libraryUrl)
  const bestUrl = libraryExists ? libraryUrl : getSteamCoverUrl(appId, 'header')

  coverUrlCache.set(appId, bestUrl)
  return bestUrl
}

/**
 * Get the best available Steam cover URL
 * @param {number} appId Steam app ID
 * @param {string} headerImage Header image URL (fetched from API)
 * @returns {Promise<string>} Best cover URL
 */
export async function getBestCoverUrl(appId, headerImage) {
  const libraryUrl = getSteamCoverUrl(appId, 'library')
  const libraryExists = await checkImageExists(libraryUrl)
  return libraryExists ? libraryUrl : headerImage || getSteamCoverUrl(appId, 'header')
}

/**
 * Batch get Steam cover URLs (optimized)
 * @param {Array<number>} appIds Array of Steam app IDs
 * @returns {Promise<Map<number, string>>} Map from appId to cover URL
 */
export async function batchGetCoverUrls(appIds) {
  const results = new Map()
  const uncachedIds = []

  for (const appId of appIds) {
    if (coverUrlCache.has(appId)) {
      results.set(appId, coverUrlCache.get(appId))
    } else {
      uncachedIds.push(appId)
    }
  }

  if (uncachedIds.length > 0) {
    const fetched = await Promise.all(
      uncachedIds.map(async (appId) => ({
        appId,
        url: await getCachedBestCoverUrl(appId),
      }))
    )
    fetched.forEach(({ appId, url }) => results.set(appId, url))
  }

  return results
}

/**
 * Clear cover URL caches
 */
export function clearCoverCache() {
  coverUrlCache.clear()
  steamGridDBCache.clear()
  imageExistsCache.clear()
}

/**
 * Validate Steam app ID
 * @param {number|string} appId App ID
 * @returns {boolean} Whether it is valid
 */
export function isValidSteamAppId(appId) {
  const id = parseInt(appId)
  return !isNaN(id) && id > 0 && id < 2147483647
}

/**
 * Format Steam app info
 * @param {Object} appData Steam app data
 * @returns {Object} Formatted app info
 */
export function formatSteamAppInfo(appData) {
  return {
    id: appData.steam_appid,
    name: appData.name,
    type: appData.type,
    description: appData.short_description,
    developers: appData.developers || [],
    publishers: appData.publishers || [],
    releaseDate: appData.release_date?.date || null,
    price: appData.price_overview || null,
    categories: appData.categories || [],
    genres: appData.genres || [],
    screenshots: appData.screenshots || [],
    movies: appData.movies || [],
    achievements: appData.achievements || [],
    platforms: appData.platforms || {},
    metacritic: appData.metacritic || null,
    recommendations: appData.recommendations || null,
  }
}

// ==================== SteamGridDB support ====================

/**
 * Generic SteamGridDB resource mapping function
 * @param {Object} item Resource item
 * @returns {Object} Mapped object
 */
function mapSteamGridDBItem(item) {
  return {
    id: item.id,
    url: item.url,
    thumb: item.thumb,
    width: item.width,
    height: item.height,
    style: item.style,
    nsfw: item.nsfw,
    humor: item.humor,
    author: item.author,
    language: item.language,
    score: item.score,
    ...(item.lock !== undefined && { lock: item.lock }),
    ...(item.epilepsy !== undefined && { epilepsy: item.epilepsy }),
  }
}

/**
 * Generic SteamGridDB resource fetcher
 * @param {string} resourceType Resource type (grids, heroes, logos, icons)
 * @param {number} gameId Game ID
 * @param {Object} options Options
 * @returns {Promise<Array>} Resource list
 */
async function fetchSteamGridDBResource(resourceType, gameId, options = {}) {
  if (!gameId) {
    return []
  }

  const queryString = buildQueryString(options)
  const url = `${STEAMGRIDDB_API_BASE}/${resourceType}/game/${gameId}${queryString}`
  const data = await fetchJson(url)

  if (!data?.success || !data?.data) {
    return []
  }

  return data.data.map(mapSteamGridDBItem)
}

/**
 * Search games on SteamGridDB
 * @param {string} searchTerm Search term
 * @returns {Promise<Array>} Game list
 */
export async function searchSteamGridDB(searchTerm) {
  if (!searchTerm?.trim()) {
    return []
  }

  const data = await fetchJson(`${STEAMGRIDDB_API_BASE}/search/autocomplete/${encodeURIComponent(searchTerm)}`)

  if (!data?.success || !data?.data) {
    return []
  }

  return data.data.map((game) => ({
    id: game.id,
    name: game.name,
    releaseDate: game.release_date,
    types: game.types || [],
    verified: game.verified || false,
  }))
}

/**
 * Get the SteamGridDB game ID from a Steam AppID
 * @param {number} steamAppId Steam app ID
 * @returns {Promise<number|null>} SteamGridDB game ID
 */
export async function getSteamGridDBGameId(steamAppId) {
  const cacheKey = `steam_${steamAppId}`
  if (steamGridDBCache.has(cacheKey)) {
    return steamGridDBCache.get(cacheKey)
  }

  const data = await fetchJson(`${STEAMGRIDDB_API_BASE}/games/steam/${steamAppId}`)

  if (data?.success && data?.data) {
    const gameId = data.data.id
    steamGridDBCache.set(cacheKey, gameId)
    return gameId
  }

  return null
}

/**
 * Get SteamGridDB covers (Grids)
 * @param {number} gameId SteamGridDB game ID
 * @param {Object} options Options
 * @returns {Promise<Array>} Cover list
 */
export function getSteamGridDBGrids(gameId, options = {}) {
  return fetchSteamGridDBResource('grids', gameId, options)
}

/**
 * Get SteamGridDB hero images (Heroes)
 * @param {number} gameId SteamGridDB game ID
 * @param {Object} options Options
 * @returns {Promise<Array>} Hero image list
 */
export function getSteamGridDBHeroes(gameId, options = {}) {
  return fetchSteamGridDBResource('heroes', gameId, options)
}

/**
 * Get SteamGridDB logos
 * @param {number} gameId SteamGridDB game ID
 * @param {Object} options Options
 * @returns {Promise<Array>} Logo list
 */
export function getSteamGridDBLogos(gameId, options = {}) {
  return fetchSteamGridDBResource('logos', gameId, options)
}

/**
 * Get SteamGridDB icons
 * @param {number} gameId SteamGridDB game ID
 * @param {Object} options Options
 * @returns {Promise<Array>} Icon list
 */
export function getSteamGridDBIcons(gameId, options = {}) {
  return fetchSteamGridDBResource('icons', gameId, options)
}

// Default SteamGridDB options
const DEFAULT_GRID_OPTIONS = {
  dimensions: '600x900',
  types: 'static',
  nsfw: 'false',
  humor: 'false',
}

/**
 * Search SteamGridDB covers (combined search)
 * @param {string} name Game name
 * @param {number} maxResults Maximum number of results
 * @param {Object} gridOptions Cover options
 * @returns {Promise<Array>} Cover list
 */
export async function searchSteamGridDBCovers(name, maxResults = 20, gridOptions = {}) {
  if (!name) {
    return []
  }

  const games = await searchSteamGridDB(name)

  if (!games.length) {
    return []
  }

  const options = { ...DEFAULT_GRID_OPTIONS, ...gridOptions }

  const coverPromises = games.slice(0, 10).map(async (game) => {
    const grids = await getSteamGridDBGrids(game.id, options)

    return grids.slice(0, 3).map((grid) => ({
      name: game.name,
      gameId: game.id,
      source: 'steamgriddb',
      url: grid.thumb || grid.url,
      saveUrl: grid.url,
      key: `sgdb_${game.id}_${grid.id}`,
      width: grid.width,
      height: grid.height,
      style: grid.style,
      author: grid.author,
      score: grid.score,
    }))
  })

  const results = await Promise.all(coverPromises)
  return results.flat().slice(0, maxResults)
}

/**
 * Get SteamGridDB covers from a Steam AppID
 * @param {number} steamAppId Steam app ID
 * @param {Object} gridOptions Cover options
 * @returns {Promise<Array>} Cover list
 */
export async function getSteamGridDBCoversBySteamId(steamAppId, gridOptions = {}) {
  const gameId = await getSteamGridDBGameId(steamAppId)

  if (!gameId) {
    return []
  }

  const options = { ...DEFAULT_GRID_OPTIONS, ...gridOptions }
  const grids = await getSteamGridDBGrids(gameId, options)

  return grids.map((grid) => ({
    gameId,
    steamAppId,
    source: 'steamgriddb',
    url: grid.thumb || grid.url,
    saveUrl: grid.url,
    key: `sgdb_${gameId}_${grid.id}`,
    width: grid.width,
    height: grid.height,
    style: grid.style,
    author: grid.author,
    score: grid.score,
  }))
}

export default {
  loadSteamApps,
  searchSteamApps,
  getSteamAppDetails,
  searchSteamCovers,
  searchSteamCoversWithDetails,
  getSteamCoverUrl,
  checkImageExists,
  getBestCoverUrl,
  getCachedBestCoverUrl,
  batchGetCoverUrls,
  clearCoverCache,
  isValidSteamAppId,
  formatSteamAppInfo,
  // SteamGridDB
  searchSteamGridDB,
  getSteamGridDBGameId,
  getSteamGridDBGrids,
  getSteamGridDBHeroes,
  getSteamGridDBLogos,
  getSteamGridDBIcons,
  searchSteamGridDBCovers,
  getSteamGridDBCoversBySteamId,
}
