# Utility Modules

## File Selection Module (fileSelection.js)

Cross-platform file and directory selection, supporting both Electron and browser environments.

### Quick start

```javascript
import { createFileSelector } from './utils/fileSelection.js';

// Create a file-selector instance
const fileSelector = createFileSelector({
  platform: 'windows', // 'windows', 'linux', 'macos'
  onSuccess: (message) => console.log(message),
  onError: (error) => console.error(error),
  onInfo: (info) => console.info(info)
});

// Select a file
fileSelector.selectFile('cmd', fileInputRef, (fieldName, filePath) => {
  console.log(`Selected file: ${filePath}`);
});

// Select a directory
fileSelector.selectDirectory('working-dir', dirInputRef, (fieldName, dirPath) => {
  console.log(`Selected directory: ${dirPath}`);
});
```

### Simplified usage

```javascript
import { selectFile, selectDirectory } from './utils/fileSelection.js';

// Select a file directly
selectFile({
  fieldName: 'cmd',
  fileInput: fileInputRef,
  platform: 'windows',
  callback: (fieldName, filePath) => {
    // handle the selected file
  }
});

// Select a directory directly
selectDirectory({
  fieldName: 'working-dir',
  dirInput: dirInputRef,
  platform: 'windows',
  callback: (fieldName, dirPath) => {
    // handle the selected directory
  }
});
```

### Environment detection

```javascript
import { checkEnvironmentSupport } from './utils/fileSelection.js';

const support = checkEnvironmentSupport();
console.log('File selection support:', support.fileSelection);
console.log('Directory selection support:', support.directorySelection);
console.log('Electron environment:', support.isElectron);
console.log('Development environment:', support.isDevelopment);
```

### Using inside a Vue component

```javascript
import { createFileSelector } from '../utils/fileSelection.js';

export default {
  data() {
    return {
      fileSelector: null
    };
  },
  
  mounted() {
    this.fileSelector = createFileSelector({
      platform: this.platform,
      onSuccess: this.showSuccess,
      onError: this.showError,
      onInfo: this.showInfo
    });
  },
  
  methods: {
    selectFile(fieldName) {
      this.fileSelector.selectFile(
        fieldName,
        this.$refs.fileInput,
        this.onFileSelected
      );
    },
    
    onFileSelected(fieldName, filePath) {
      this.formData[fieldName] = filePath;
      this.validateField(fieldName);
    }
  }
};
```

### Supported platforms

- **Electron**: native file / directory selection dialogs
- **Browser**: HTML5 file API, with security restrictions
- **Development**: a simulated full path for easier testing

### Notes

- The browser environment cannot read full system paths
- The development environment automatically simulates a full path
- The Electron environment provides the best user experience

## Form Validation Module (validation.js)

Form-field validation utilities.

### Usage

```javascript
import { validateField, validateAppForm } from './utils/validation.js';

// Validate a single field
const result = validateField('appName', 'MyApp');
console.log(result.isValid); // true/false
console.log(result.message); // error message

// Validate an entire form
const formResult = validateAppForm(formData);
console.log(formResult.isValid); // true/false
console.log(formResult.errors); // list of errors
```

## Steam API Module (steamApi.js)

Integration with the Steam Store API.

### Usage

```javascript
import { searchSteamApps, findAppCover } from './utils/steamApi.js';

// Search for Steam apps
const apps = await searchSteamApps('Half-Life');

// Find a cover image for an app
const cover = await findAppCover('Half-Life 2');
```
