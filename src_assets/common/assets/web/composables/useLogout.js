/**
 * Logout composable
 */
export function useLogout() {
  /**
   * @param { { onLocalhost?: () => void } } [opts]
   */
  const logout = (opts = {}) => {
    const xhr = new XMLHttpRequest()
    xhr.open('GET', '/api/logout?t=' + Date.now(), true)
    xhr.setRequestHeader('Authorization', 'Basic ' + btoa('logout:logout'))

    // The browser can get stuck retrying on 401 and never call back into JS,
    // so we don't wait. After 200ms we force a navigation back to the home page.
    // This breaks out of the browser's XHR retry loop.
    const watchdog = setTimeout(() => {
      // If we reach here, either 200 never fired or the browser is stuck on 401.
      // Force-abort the request and redirect to the home page.
      try { xhr.abort() } catch(e) {}
      window.location.href = '/'
    }, 200)

    xhr.onreadystatechange = () => {
      // The only case where we cancel the redirect: the backend explicitly returned 200 (Localhost)
      if (xhr.readyState === 4 && xhr.status === 200) {
        clearTimeout(watchdog)
        opts.onLocalhost?.()   // Execute the Localhost branch
      }
    }
    xhr.send()
  }
  return { logout }
}