import {createI18n} from "vue-i18n";

// Import only the fallback language files
import en from '../public/assets/locale/en.json'

export default async function() {
    // First try to get the live configuration from /api/config (this reads the config file)
    let locale = "en";
    try {
        let config = await (await fetch("/api/config")).json();
        locale = config.locale ?? "en";
    } catch (e) {
        // If that fails, fall back to /api/configLocale (read from memory)
        try {
            let r = await (await fetch("/api/configLocale")).json();
            locale = r.locale ?? "en";
        } catch (e2) {
            console.error("Failed to get locale config", e, e2);
        }
    }
    
    document.querySelector('html').setAttribute('lang', locale);
    let messages = {
        en
    };
    try {
        if (locale !== 'en') {
            let r = await (await fetch(`/assets/locale/${locale}.json`)).json();
            messages[locale] = r;
        }
    } catch (e) {
        console.error("Failed to download translations", e);
    }
    const i18n = createI18n({
        legacy: false, // Use Composition API mode
        locale: locale, // set locale
        fallbackLocale: 'en', // set fallback locale
        messages: messages,
        globalInjection: true, // Allow using $t in templates
        warnHtmlMessage: false, // Disable HTML message warnings (we use v-html to render trusted translation content)
    })
    return i18n;
}
