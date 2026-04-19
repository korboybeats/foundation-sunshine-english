import i18n from './config/i18n.js'

// must import even if not implicitly using here
// https://github.com/aurelia/skeleton-navigation/issues/894
// https://discourse.aurelia.io/t/bootstrap-import-bootstrap-breaks-dropdown-menu-in-navbar/641/9
// Import Bootstrap and manually set it on the global object (ES modules don't auto-register on window)
import * as bootstrap from 'bootstrap'

// Set Bootstrap on the global object so it can be used in components
if (typeof window !== 'undefined') {
  window.bootstrap = bootstrap
}

export function initApp(app, config) {
    //Wait for locale initialization, then render
    i18n().then(i18n => {
        app.use(i18n);
        app.provide('i18n', i18n.global)
        app.mount('#app');
        if (config) {
            config(app)
        }
    });
}
