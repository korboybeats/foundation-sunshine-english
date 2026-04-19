const getStoredTheme = () => localStorage.getItem('theme')
export const setStoredTheme = (theme) => localStorage.setItem('theme', theme)

export const getPreferredTheme = () => {
  const storedTheme = getStoredTheme()
  if (storedTheme) {
    return storedTheme
  }

  return window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light'
}

export const setTheme = (theme) => {
  if (theme === 'auto') {
    document.documentElement.setAttribute(
      'data-bs-theme',
      window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light'
    )
  } else {
    document.documentElement.setAttribute('data-bs-theme', theme)
  }
}

export const showActiveTheme = (theme, focus = false) => {
  const themeSwitcher = document.querySelector('#bd-theme')

  if (!themeSwitcher) {
    return
  }

  const themeSwitcherText = document.querySelector('#bd-theme-text')
  const activeThemeIcon = document.querySelector('.theme-icon-active i')
  const btnToActive = document.querySelector(`[data-bs-theme-value="${theme}"]`)
  
  if (!btnToActive) {
    return
  }
  
  const classListOfActiveBtn = btnToActive.querySelector('i').classList

  document.querySelectorAll('[data-bs-theme-value]').forEach((element) => {
    element.classList.remove('active')
    element.setAttribute('aria-pressed', 'false')
  })

  btnToActive.classList.add('active')
  btnToActive.setAttribute('aria-pressed', 'true')
  activeThemeIcon.classList.remove(...activeThemeIcon.classList.values())
  activeThemeIcon.classList.add(...classListOfActiveBtn)
  const themeSwitcherLabel = `${themeSwitcherText.textContent} (${btnToActive.textContent.trim()})`
  themeSwitcher.setAttribute('aria-label', themeSwitcherLabel)

  if (focus) {
    themeSwitcher.focus()
  }
}

// Singleton flag to ensure global event listeners are added only once
let isAutoThemeInitialized = false
let mediaQueryHandler = null
let domContentLoadedHandler = null

export function loadAutoTheme() {
  // Set the theme
  setTheme(getPreferredTheme())

  // Only attach global event listeners on the first call
  if (!isAutoThemeInitialized) {
    // Handle system theme changes
    const mediaQuery = window.matchMedia('(prefers-color-scheme: dark)')
    mediaQueryHandler = () => {
    const storedTheme = getStoredTheme()
    if (storedTheme !== 'light' && storedTheme !== 'dark') {
      setTheme(getPreferredTheme())
    }
    }
    mediaQuery.addEventListener('change', mediaQueryHandler)

    // Handle DOMContentLoaded (if the document is already loaded, run immediately)
    domContentLoadedHandler = () => {
    showActiveTheme(getPreferredTheme())
    }
    if (document.readyState === 'loading') {
      window.addEventListener('DOMContentLoaded', domContentLoadedHandler)
    } else {
      // Document is already loaded, execute directly
      domContentLoadedHandler()
    }

    isAutoThemeInitialized = true
  }
}
