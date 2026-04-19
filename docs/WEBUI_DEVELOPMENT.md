# WebUI Development Guide

Sunshine includes a modern web control interface, built on Vue 3 and the Composition API and following Vue best practices.

> **Note**: This document has been updated to reflect the latest project structure improvements. All pages have been refactored to use the Composition API and a modular architecture.

## 🛠️ Tech Stack

- **Frontend framework**: Vue 3 + Composition API
- **Build tool**: Vite 5.4+ (Rolldown supported)
- **Bundler**: Rolldown (experimental, faster)
- **UI components**: Bootstrap 5
- **Icon library**: FontAwesome 6
- **Internationalization**: Vue-i18n 11 (Composition API mode)
- **Drag and drop**: Vuedraggable 4
- **Module system**: ES Modules (`"type": "module"`)

> **Note**: This document has been updated to reflect the latest project structure improvements. All pages have been refactored to use the Composition API and a modular architecture.

## 🚀 Setting Up the Development Environment

### 1. Install dependencies

```bash
npm install
```

### 2. Development commands

```bash
# Development mode — incremental build with file watching
npm run dev

# Dev server — start an HTTPS development server (recommended)
npm run dev-server

# Full development environment — includes mock API services
npm run dev-full

# Build the production bundle
npm run build

# Clean the build directory and rebuild
npm run build-clean

# Preview the production build
npm run preview

# Build and preview the production bundle in one step (recommended)
npm run preview:build
```

> **Note**: The project is configured to use Rolldown (Vite 5.1+'s experimental bundler) for faster builds. All build commands have Rolldown enabled by default.

### 3. Dev server features

- **HTTPS support**: a local SSL certificate is generated automatically
- **Hot reload**: live updates on code changes
- **Proxy configuration**: API requests are automatically proxied to the Sunshine service
- **Mock data**: mock API responses are available in development mode
- **Port**: defaults to `https://localhost:3000`

## 📁 Project Structure

```
src_assets/common/assets/web/
├── views/                    # Page components (route-level components)
│   ├── Home.vue             # Home page
│   ├── Apps.vue             # Application management page
│   ├── Config.vue           # Configuration management page
│   ├── Troubleshooting.vue  # Troubleshooting page
│   ├── Pin.vue              # PIN pairing page
│   ├── Password.vue         # Password change page
│   └── Welcome.vue          # Welcome page
│
├── components/              # Vue components
│   ├── layout/              # Layout components
│   │   ├── Navbar.vue       # Navigation bar
│   │   └── PlatformLayout.vue # Platform layout component
│   ├── common/              # Shared components
│   │   ├── ThemeToggle.vue  # Theme switcher
│   │   ├── ResourceCard.vue # Resource card
│   │   ├── VersionCard.vue  # Version-info card
│   │   ├── ErrorLogs.vue    # Error log component
│   │   └── Locale.vue       # Locale component
│   ├── SetupWizard.vue      # Setup wizard
│   └── ...                  # Other feature components
│
├── composables/             # Composables (reusable logic)
│   ├── useVersion.js        # Version management
│   ├── useLogs.js           # Log management
│   ├── useSetupWizard.js    # Setup wizard logic
│   ├── useApps.js           # Application management
│   ├── useConfig.js         # Configuration management
│   ├── useTroubleshooting.js # Troubleshooting
│   ├── usePin.js            # PIN pairing
│   ├── useWelcome.js        # Welcome page
│   └── useTheme.js          # Theme management
│
├── config/                  # Configuration files
│   ├── firebase.js          # Firebase configuration
│   └── i18n.js              # Internationalization configuration
│
├── services/                # API services
│   └── appService.js        # Application service
│
├── utils/                   # Utility functions
│   ├── constants.js         # Constants
│   ├── helpers.js           # Helper functions
│   ├── validation.js        # Form validation
│   ├── theme.js             # Theme utilities
│   └── ...
│
├── styles/                  # Stylesheets
│   ├── apps.css             # Application page styles
│   ├── welcome.css          # Welcome page styles
│   └── ...
│
├── public/                  # Static assets
│   ├── assets/
│   │   ├── css/             # Global styles
│   │   └── locale/          # i18n files
│   └── images/              # Image assets
│
├── configs/                 # Sub-components for the config page
│   └── tabs/                # Configuration tab components
│
├── *.html                   # Page entry files (simplified)
└── init.js                  # Application initialization
```

## 🎯 Architectural Principles

### 1. Directory organization

- **views/**: page-level components, each mapped to a route
- **components/layout/**: layout-related components (Navbar, PlatformLayout)
- **components/common/**: reusable, generic components
- **components/**: feature-specific components
- **composables/**: reusable business logic
- **config/**: configuration files
- **services/**: API service layer
- **utils/**: pure utility functions

### 2. Component categories

#### Page components (views/)
- Correspond to a complete page
- Use the Composition API (`<script setup>`)
- Compose child components and composables
- Manage page-level state and lifecycle

#### Layout components (components/layout/)
- Page-layout related (e.g. navigation bar)
- Reusable across pages

#### Shared components (components/common/)
- Highly reusable UI components
- No business logic, or only trivial logic

#### Feature components (components/)
- Components tied to a specific feature
- Contain a meaningful amount of business logic

### 3. Composable design

Composables extract reusable business logic:

```javascript
// composables/useExample.js
import { ref, computed } from 'vue'

export function useExample() {
  const data = ref(null)
  const loading = ref(false)
  
  const computedValue = computed(() => {
    // computation
  })
  
  const fetchData = async () => {
    // data fetching
  }
  
  return {
    data,
    loading,
    computedValue,
    fetchData,
  }
}
```

## 📝 Development Conventions

### 1. Creating a new page

#### Step 1: Create the page component

```vue
<!-- views/NewPage.vue -->
<template>
  <div>
    <Navbar />
    <div class="container">
      <h1>{{ $t('newpage.title') }}</h1>
      <!-- page content -->
    </div>
  </div>
</template>

<script setup>
import Navbar from '../components/layout/Navbar.vue'
// import the composables you need
import { useNewPage } from '../composables/useNewPage.js'

const {
  // destructure the state and methods you need
} = useNewPage()
</script>

<style scoped>
/* page-specific styles */
</style>
```

#### Step 2: Create a composable (if needed)

```javascript
// composables/useNewPage.js
import { ref, computed } from 'vue'

export function useNewPage() {
  const data = ref(null)
  
  const fetchData = async () => {
    // data fetching
  }
  
  return {
    data,
    fetchData,
  }
}
```

#### Step 3: Create the HTML entry file

```html
<!-- newpage.html -->
<!DOCTYPE html>
<html lang="en" data-bs-theme="auto">
  <head>
    <%- header %>
  </head>

  <body id="app" v-cloak>
    <!-- Vue mount point -->
  </body>

  <script type="module">
    import { createApp } from 'vue'
    import { initApp } from './init'
    import NewPage from './views/NewPage.vue'

    const app = createApp(NewPage)
    initApp(app)
  </script>
</html>
```

### 2. Using the Composition API

**Prefer the `<script setup>` syntax:**

```vue
<script setup>
import { ref, computed, onMounted } from 'vue'
import { useI18n } from 'vue-i18n'

const { t } = useI18n()
const count = ref(0)

const doubleCount = computed(() => count.value * 2)

onMounted(() => {
  // initialization
})
</script>
```

### 3. Internationalization

#### In templates

```vue
<template>
  <div>
    <!-- Use $t in templates (via globalInjection) -->
    <h1>{{ $t('common.title') }}</h1>
    <p>{{ $t('common.description') }}</p>
    
    <!-- Use in attributes -->
    <input :placeholder="$t('common.placeholder')" />
    <button :title="$t('common.tooltip')">{{ $t('common.button') }}</button>
  </div>
</template>

<script setup>
import { useI18n } from 'vue-i18n'
// In script, use useI18n() to get the t function
const { t } = useI18n()
</script>
```

#### In `<script setup>`

When you need translations in JavaScript (e.g. `alert()`, `confirm()`), you must use `useI18n()`:

```vue
<script setup>
import { useI18n } from 'vue-i18n'

const { t } = useI18n()

// Use it in functions
const handleConfirm = () => {
  if (confirm(t('common.confirm_message'))) {
    // handle confirmation
  }
}

const showError = () => {
  alert(t('common.error_message'))
}
</script>
```

#### In composables

```javascript
import { useI18n } from 'vue-i18n'

export function useExample() {
  const { t } = useI18n()
  
  const showMessage = (key) => {
    alert(t(key))
  }
  
  return { showMessage }
}
```

### 4. Style organization

- **Global styles**: `public/assets/css/` or `styles/`
- **Component styles**: use `<style scoped>` inside the component
- **Page-specific styles**: kept in the corresponding page component

### 5. API calls

Organize API calls under `services/`:

```javascript
// services/exampleService.js
export class ExampleService {
  static async getData() {
    const response = await fetch('/api/example')
    return response.json()
  }
  
  static async saveData(data) {
    const response = await fetch('/api/example', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(data),
    })
    return response.json()
  }
}
```

## 🚀 Development Workflow

### 1. Building a new feature

1. **Analyze the requirements**: decide whether it is a page, a component, or an enhancement
2. **Create composables**: extract reusable business logic
3. **Create components**: implement the UI and interactions
4. **Create the page**: compose components and composables
5. **Add the route**: create the HTML entry file
6. **Test and verify**: make sure the feature works

### 2. Code review checklist

- ✅ Follows the directory structure conventions
- ✅ Uses the Composition API
- ✅ Business logic is extracted into composables
- ✅ Components are reusable
- ✅ Styles are sensibly organized
- ✅ Required error handling is in place

## 📚 Example Code

### Complete page example

```vue
<!-- views/Example.vue -->
<template>
  <div>
    <Navbar />
    <div class="container">
      <h1>{{ $t('example.title') }}</h1>
      
      <ExampleCard 
        v-for="item in items" 
        :key="item.id"
        :item="item"
        @action="handleAction"
      />
      
      <div v-if="loading" class="text-center">
        <div class="spinner-border" role="status">
          <span class="visually-hidden">Loading...</span>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { onMounted } from 'vue'
import Navbar from '../components/layout/Navbar.vue'
import ExampleCard from '../components/ExampleCard.vue'
import { useExample } from '../composables/useExample.js'
import { trackEvents } from '../config/firebase.js'

const {
  items,
  loading,
  fetchItems,
  handleAction,
} = useExample()

onMounted(async () => {
  trackEvents.pageView('example')
  await fetchItems()
})
</script>

<style scoped>
.container {
  padding: 1rem;
}
</style>
```

### Composable example

```javascript
// composables/useExample.js
import { ref, computed } from 'vue'
import { ExampleService } from '../services/exampleService.js'
import { trackEvents } from '../config/firebase.js'

export function useExample() {
  const items = ref([])
  const loading = ref(false)
  const error = ref(null)
  
  const itemCount = computed(() => items.value.length)
  
  const fetchItems = async () => {
    loading.value = true
    error.value = null
    try {
      items.value = await ExampleService.getItems()
      trackEvents.userAction('items_loaded', { count: items.value.length })
    } catch (err) {
      error.value = err.message
      trackEvents.errorOccurred('fetch_items', err.message)
    } finally {
      loading.value = false
    }
  }
  
  const handleAction = async (itemId) => {
    try {
      await ExampleService.performAction(itemId)
      await fetchItems() // refresh the list
    } catch (err) {
      console.error('Action failed:', err)
    }
  }
  
  return {
    items,
    loading,
    error,
    itemCount,
    fetchItems,
    handleAction,
  }
}
```

## 🔧 Configuration Notes

### i18n configuration

```javascript
// config/i18n.js
const i18n = createI18n({
  legacy: false,           // use Composition API mode
  locale: locale,
  fallbackLocale: 'en',
  messages: messages,
  globalInjection: true,   // allow $t in templates
})
```

### Firebase configuration

```javascript
// config/firebase.js
import { initFirebase, trackEvents } from './config/firebase.js'

// initialize
initFirebase()

// usage
trackEvents.pageView('page_name')
trackEvents.userAction('action_name', { data })
trackEvents.gpuReported({ platform: 'windows', adapters: [...] })
```

**Available events**:
- `pageView(pageName)` — page view
- `userAction(actionName, data)` — user action
- `errorOccurred(errorType, message)` — error occurrence
- `gpuReported(gpuInfo)` — GPU info reporting (only reported once per 24 hours)

## 🎨 Style Guide

### Using Bootstrap 5

The project uses Bootstrap 5 as its UI framework. Prefer Bootstrap components and utility classes wherever possible.

### Custom styles

- Use `<style scoped>` for component-specific styles
- Put global styles under `styles/`
- Use CSS variables for theming

## 📦 Dependency Management

Main dependencies:
- `vue` — Vue 3 framework
- `vue-i18n` — internationalization (Composition API mode)
- `bootstrap` — UI framework
- `vuedraggable` — drag and drop
- `marked` — Markdown parsing

## 🐛 Debugging Tips

1. **Use Vue DevTools**: install the Vue DevTools browser extension
2. **Console logging**: use `console.log` for ad-hoc debugging
3. **Network requests**: inspect API requests in the browser dev tools
4. **Component inspection**: inspect component state in Vue DevTools

## 🔧 Build Configuration

### Vite configuration

- **Dev configuration**: `vite.dev.config.js` — development-only configuration
- **Production configuration**: `vite.config.js` — production build configuration
- **EJS templates**: HTML template preprocessing supported
- **Path aliases**: aliases for Vue and Bootstrap are configured
- **Rolldown support**: uses Rolldown as the experimental bundler (faster)
- **ESM mode**: the project uses ES modules (`"type": "module"`)

### Proxy configuration

The dev server includes the following proxies:
- `/api/*` → `https://localhost:47990` (Sunshine API)
- `/steam-api/*` → Steam API service
- `/steam-store/*` → Steam Store service

### Preview mode

Preview mode is for testing the production build, but be aware that:

1. **No API available**: there is no backend API server in preview mode
2. **Error handling**: the code is written to degrade gracefully under preview mode
3. **Use case**: mainly for verifying the build artifacts and static assets

```bash
# Build and preview
npm run preview:build

# Or step by step
npm run build
npm run preview
```

URL: `http://localhost:3000`

### Code-splitting strategy

> **Note**: manual chunking (`manualChunks`) is currently disabled because it can break the dependency relationship between Bootstrap and Popper.js, breaking features like dropdown menus. Vite handles code splitting automatically.

## 🌍 Internationalization Support

- Multi-language switching
- Built on Vue-i18n 11 (Composition API mode)
- Locale files live under `public/assets/locale/`
- Configured in `config/i18n.js`

### i18n development workflow

The project ships with a complete i18n toolchain to keep translation files in good shape. The reference language file is `en.json`, and all other language files must stay in sync with it.

#### Available commands

```bash
# Validate the integrity of all language files
npm run i18n:validate

# Auto-sync missing keys (only adds keys; missing values are filled with the English placeholder)
# Note: sync only ensures "the keys are complete"; it does not translate. The English placeholders
# in other language files still need to be manually translated.
npm run i18n:sync

# Format and sort all language files (alphabetically)
npm run i18n:format

# Check the file format
npm run i18n:format:check

# Validate translation completeness
npm run i18n:validate
```

#### Adding new translation keys

1. **Add the new key to the reference file**: add the key and English value to `en.json` first
   ```json
   {
     "myfeature": {
       "title": "My Feature Title",
       "description": "My feature description",
       "button_label": "Submit"
     }
   }
   ```

2. **Sync to other language files**:
   ```bash
   npm run i18n:sync
   ```
   This automatically adds the missing keys to all language files, using the English value as a placeholder. **Sync only fills in the keys; it does not translate.** You still need to manually replace the English placeholder values in each language file with proper translations.

3. **Format the files**:
   ```bash
   npm run i18n:format
   ```
   This sorts and formats all language files consistently, reducing Git conflicts.

4. **Translate the placeholders (required)**: manually replace the English placeholders that `sync` inserted with proper translations for each language. Until you do, the UI will display English.

5. **Validate**:
   ```bash
   npm run i18n:validate
   ```
   Confirms that every language file contains all the required translation keys.

#### Internationalizing an existing component

A complete worked example follows.

**Step 1: Identify hardcoded text**
```vue
<!-- Original component -->
<template>
  <div>
    <h2>Client List</h2>
    <table>
      <thead>
        <tr>
          <th>Name</th>
          <th>Actions</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="client in clients" :key="client.id">
          <td>{{ client.name || 'Unknown Client' }}</td>
          <td>
            <button @click="handleDelete">Delete</button>
          </td>
        </tr>
      </tbody>
    </table>
  </div>
</template>

<script setup>
const handleDelete = () => {
  if (confirm('Are you sure you want to delete?')) {
    // delete logic
  }
}
</script>
```

**Step 2: Add translation keys to `en.json`**
```json
{
  "client": {
    "list_title": "Client List",
    "name": "Name",
    "actions": "Actions",
    "unknown_client": "Unknown Client",
    "delete": "Delete",
    "confirm_delete": "Are you sure you want to delete?"
  }
}
```

**Step 3: Update the component to use translations**
```vue
<template>
  <div>
    <h2>{{ $t('client.list_title') }}</h2>
    <table>
      <thead>
        <tr>
          <th>{{ $t('client.name') }}</th>
          <th>{{ $t('client.actions') }}</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="client in clients" :key="client.id">
          <td>{{ client.name || $t('client.unknown_client') }}</td>
          <td>
            <button @click="handleDelete">{{ $t('client.delete') }}</button>
          </td>
        </tr>
      </tbody>
    </table>
  </div>
</template>

<script setup>
import { useI18n } from 'vue-i18n'

const { t } = useI18n()

const handleDelete = () => {
  if (confirm(t('client.confirm_delete'))) {
    // delete logic
  }
}
</script>
```

**Step 4: Sync and validate**
```bash
npm run i18n:sync
npm run i18n:format
npm run i18n:validate
```

#### Best practices

- **Validate before committing**: run `npm run i18n:validate` before committing to make sure no translations are missing
- **Keep formatting consistent**: run `npm run i18n:format` periodically so files stay tidy
- **Avoid direct edits**: don't directly delete or rename translation keys in non-English files; modify `en.json` first and then sync
- **CI integration**: CI automatically checks the integrity and format of translation files to enforce quality

#### Script reference

- **validate-i18n.js**: verifies that every language file contains all keys defined in `en.json`, and reports any missing or extra keys
- **format-i18n.js**: alphabetizes the keys in every language file and applies consistent formatting (2-space indentation)

These tools ensure:
- ✅ All language files share the same set of translation keys
- ✅ Files have a consistent format, reducing unnecessary Git conflicts
- ✅ Missing translations are easy to spot and fix
- ✅ Code review is easier

## 🎨 Theming

- Light and dark themes are supported
- Implemented with CSS variables
- Theme utilities live in `utils/theme.js`
- Use `composables/useTheme.js` to manage themes inside components

## 📱 Responsive Design

- Bootstrap 5 responsive layout
- Supports both desktop and mobile
- Optimized touch interactions

## 🧪 Testing & Debugging

- Source maps are enabled in development
- Detailed proxy request logging
- Mock API data for frontend development
- Use Vue DevTools to inspect components

## 📦 Build & Deploy

### Build commands

```bash
# Production build
npm run build

# Build output: build/assets/web/
# Includes all static assets and HTML files
```

## 📖 Related Resources

- [Vue 3 Docs](https://vuejs.org/)
- [Vue I18n Docs](https://vue-i18n.intlify.dev/)
- [Bootstrap 5 Docs](https://getbootstrap.com/docs/5.3/)
- [Composition API Guide](https://vuejs.org/guide/extras/composition-api-faq.html)
- [Vue I18n Composition API mode](https://vue-i18n.intlify.dev/guide/advanced/composition.html)

## 🔄 Migration Guide

### Migrating from Options API to Composition API

When you encounter a legacy Options API component, migrate it like so:

1. Replace `data()` with `ref()` or `reactive()`
2. Replace `computed` with `computed()`
3. Replace `methods` with plain functions
4. Replace lifecycle hooks with their Composition API equivalents
5. Use `<script setup>` to simplify the code

### Example migration

**Before (Options API):**
```javascript
export default {
  data() {
    return {
      count: 0
    }
  },
  computed: {
    double() {
      return this.count * 2
    }
  },
  methods: {
    increment() {
      this.count++
    }
  }
}
```

**After (Composition API):**
```javascript
<script setup>
import { ref, computed } from 'vue'

const count = ref(0)
const double = computed(() => count.value * 2)
const increment = () => count.value++
</script>
```

## ✅ Best-Practices Checklist

- [ ] Use the Composition API (`<script setup>`)
- [ ] Extract business logic into composables
- [ ] Place components into the right directory by category
- [ ] Use scoped styles, or put styles in the styles directory
- [ ] Use TypeScript types where applicable
- [ ] Add error handling
- [ ] Use i18n (`$t` or `t`)
- [ ] Add appropriate user feedback
- [ ] Format code consistently
- [ ] Add comments where helpful

## 📋 Quick Reference

### File naming conventions

- **Page components**: `PascalCase.vue` (e.g. `Home.vue`, `Apps.vue`)
- **Composables**: `useXxx.js` (e.g. `useVersion.js`, `useApps.js`)
- **Service classes**: `xxxService.js` (e.g. `appService.js`)
- **Utility functions**: `camelCase.js` (e.g. `helpers.js`, `validation.js`)

### Import path conventions

```javascript
// Page components
import Navbar from '../components/layout/Navbar.vue'

// Composables
import { useVersion } from '../composables/useVersion.js'

// Services
import { AppService } from '../services/appService.js'

// Utility functions
import { debounce } from '../utils/helpers.js'

// Configuration
import { trackEvents } from '../config/firebase.js'
```

### Common composables

| Composable | Purpose | Returns |
|-----------|------|---------|
| `useVersion` | Version management | version, githubVersion, fetchVersions |
| `useLogs` | Log management | logs, fatalLogs, fetchLogs |
| `useApps` | Application management | apps, loadApps, save, editApp |
| `useConfig` | Configuration management | config, save, apply |
| `useTheme` | Theme management | - |
| `usePin` | PIN pairing | clients, unpairAll, save |

## 🎯 Next Steps

- Consider adding TypeScript support
- Consider adding unit tests
- Consider adding E2E tests
- Optimize performance (lazy loading, code splitting)

## 🤝 Contribution Guide

Contributions to the WebUI are welcome! Please make sure to:

1. **Follow the conventions**
   - Use the Composition API
   - Extract business logic into composables
   - Place components in their right category

2. **Code quality**
   - Add appropriate error handling
   - Use i18n
   - Add comments where helpful

3. **Test and verify**
   - Run the build commands before committing to confirm there are no errors
   - Test new features in different browsers

4. **Documentation**
   - Update related documentation
   - Add useful code comments

## 📝 Changelog

### Latest updates (2024)

- ✅ All pages refactored to the Composition API
- ✅ Business logic extracted into composables
- ✅ Components reorganized by feature
- ✅ Configuration files unified
- ✅ Vue I18n migrated to Composition API mode
- ✅ All HTML entry files simplified
- ✅ Vite upgraded to 5.4+ with Rolldown support
- ✅ Fixed CJS Node API deprecation warnings (added `"type": "module"`)
- ✅ Added a production preview workflow
- ✅ Improved API error handling in preview mode
- ✅ Added GPU info reporting (Firebase Analytics)
- ✅ Improved error handling in the i18n configuration
- ✅ Cross-platform environment variable support (using `cross-env`)

### Technical improvements

- **Build system**: upgraded to Vite 5.4+ with the experimental Rolldown bundler
- **Module system**: migrated to ES modules (`"type": "module"`)
- **Error handling**: improved API error handling in preview mode
- **Performance**: builds are faster thanks to Rolldown
- **Developer experience**: improved preview workflow, with one-click build-and-preview
