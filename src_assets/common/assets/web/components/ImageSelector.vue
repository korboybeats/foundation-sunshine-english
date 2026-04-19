<template>
  <div class="form-group-enhanced">
    <label for="appImagePath" class="form-label-enhanced">{{ $t('apps.image') }}</label>

    <!-- Use desktop image option -->
    <div class="form-check mb-3">
      <input
        type="checkbox"
        class="form-check-input"
        id="useDesktopImage"
        :checked="isDesktopImage"
        @change="handleDesktopImageChange"
      />
      <label for="useDesktopImage" class="form-check-label">{{ $t('apps.use_desktop_image') }}</label>
    </div>

    <!-- Image path input -->
    <div v-if="!isDesktopImage" class="input-group">
      <input
        type="file"
        class="form-control"
        @change="handleFileSelect"
        accept="image/png,image/jpg,image/jpeg,image/gif,image/bmp,image/webp"
        style="width: 90px; flex: none"
      />
      <input
        type="text"
        class="form-control form-control-enhanced monospace"
        id="appImagePath"
        :value="imagePath"
        @input="updateImagePath"
        @dragenter="handleDragEnter"
        @dragleave="handleDragLeave"
        @dragover.prevent
        @drop.prevent.stop="handleDrop"
        placeholder="Select an image file or drag one here"
      />
      <button
        class="btn btn-outline-secondary"
        type="button"
        @click="openCoverFinder"
        :disabled="!appName"
      >
        <i class="fas fa-search me-1"></i>{{ $t('apps.find_cover') }}
      </button>
    </div>

    <!-- Image preview -->
    <div v-if="!isDesktopImage && imagePath" class="image-preview-container mt-3">
      <div class="image-preview">
        <img :src="previewUrl" alt="Image preview" @error="handleImageError" />
      </div>
      <div class="image-preview-circle">
        <img :src="previewUrl" alt="Image preview" @error="handleImageError" />
      </div>
    </div>

    <div class="field-hint">{{ $t('apps.image_desc') }}</div>

    <!-- Cover finder -->
    <CoverFinder
      :visible="showCoverFinder"
      :search-term="appName"
      @close="closeCoverFinder"
      @cover-selected="handleCoverSelected"
      @loading="handleCoverLoading"
      @error="handleCoverError"
    />
  </div>
</template>

<script>
import CoverFinder from './CoverFinder.vue'
import { validateFile } from '../utils/validation.js'
import { getImagePreviewUrl } from '../utils/imageUtils.js'

export default {
  name: 'ImageSelector',
  components: {
    CoverFinder,
  },
  props: {
    imagePath: {
      type: String,
      default: '',
    },
    appName: {
      type: String,
      default: '',
    },
  },
  data() {
    return {
      showCoverFinder: false,
      coverLoading: false,
      dragCounter: 0,
    }
  },
  computed: {
    isDesktopImage() {
      return this.imagePath === 'desktop'
    },
    previewUrl() {
      return getImagePreviewUrl(this.imagePath)
    },
  },
  methods: {
    /**
     * Handle desktop image checkbox change
     */
    handleDesktopImageChange(event) {
      this.$emit('update-image', event.target.checked ? 'desktop' : '')
    },

    /**
     * Update image path
     */
    updateImagePath(event) {
      this.$emit('update-image', event.target.value)
    },

    /**
     * Handle file selection
     */
    async handleFileSelect(event) {
      const file = event.target.files[0]
      if (!file) return

      await this.processFile(file)
    },

    /**
     * Handle drag enter
     */
    handleDragEnter(event) {
      event.preventDefault()
      this.dragCounter++
      this.$emit('image-error', 'Drop the file here to upload')
    },

    /**
     * Handle drag leave
     */
    handleDragLeave(event) {
      event.preventDefault()
      this.dragCounter--
      if (this.dragCounter === 0) {
        this.$emit('image-error', '')
      }
    },

    /**
     * Handle drop
     */
    async handleDrop(event) {
      event.preventDefault()
      this.dragCounter = 0

      const file = event.dataTransfer.files[0]
      if (!file) {
        this.$emit('image-error', 'Please drop the file inside the input area')
        return
      }

      await this.processFile(file)
    },

    /**
     * Handle file upload
     */
    async processFile(file) {
      const validation = validateFile(file)
      if (!validation.isValid) {
        this.$emit('image-error', validation.message)
        return
      }

      try {
        this.$emit('image-error', 'Uploading image...')
        const path = await this.uploadImageToSunshine(file)
        this.$emit('update-image', path)
        this.$emit('image-error', '')
      } catch (error) {
        console.error('Failed to upload image:', error)
        this.$emit('image-error', `Failed to upload image: ${error.message}`)
      }
    },

    /**
     * Upload image to the Sunshine API
     */
    async uploadImageToSunshine(file) {
      const base64Data = await this.readFileAsBase64(file)
      const key = this.generateImageKey()

      const response = await fetch('/api/covers/upload', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ key, data: base64Data }),
      })

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`)
      }

      const result = await response.json()
      console.log('✅ Sunshine API upload succeeded, file path:', result.path)

      return `${key}.png`
    },

    /**
     * Read a file as Base64
     */
    readFileAsBase64(file) {
      return new Promise((resolve, reject) => {
        const reader = new FileReader()
        reader.onload = () => resolve(reader.result.split(',')[1])
        reader.onerror = reject
        reader.readAsDataURL(file)
      })
    },

    /**
     * Generate the image key
     */
    generateImageKey() {
      const timestamp = Date.now()
      const appName = this.appName || 'custom'
      return `app_${appName}_${timestamp}`.replace(/[^a-zA-Z0-9_-]/g, '_')
    },

    /**
     * Get the image preview URL
     */
    getImagePreviewUrl() {
      return getImagePreviewUrl(this.imagePath)
    },

    /**
     * Handle image load error
     */
    handleImageError() {
      this.$emit('image-error', 'Failed to load image; please check the file path')
    },

    /**
     * Open the cover finder
     */
    openCoverFinder() {
      if (!this.appName) {
        this.$emit('image-error', 'Please enter the application name first')
        return
      }
      this.showCoverFinder = true
    },

    /**
     * Close the cover finder
     */
    closeCoverFinder() {
      this.showCoverFinder = false
    },

    /**
     * Handle cover selection
     */
    handleCoverSelected(coverData) {
      this.$emit('update-image', coverData.path)
      this.showCoverFinder = false
    },

    /**
     * Handle cover loading state
     */
    handleCoverLoading(loading) {
      this.coverLoading = loading
    },

    /**
     * Handle cover error
     */
    handleCoverError(error) {
      this.$emit('image-error', error)
    },
  },
}
</script>

<style scoped>
.monospace {
  font-family: monospace;
}

.image-preview-container {
  display: flex;
  align-items: center;
  justify-content: center;
}

.image-preview {
  max-width: 300px;
  max-height: 200px;
  border-radius: 0.375rem;
  padding: 1rem;
  text-align: center;
}

.image-preview img {
  max-width: 100%;
  max-height: 150px;
  object-fit: contain;
  border-radius: 0.25rem;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
}

.image-preview-circle {
  width: 150px;
  height: 150px;
  border-radius: 50%;
  padding: 1px;
  text-align: center;
  overflow: hidden;
  position: relative;
  background-color: #f8f9fa;
  border: 1px solid #dee2e6;
}

.image-preview-circle img {
  width: 98%;
  height: 98%;
  object-fit: cover;
  border-radius: 50%;
  box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
}

.image-preview-circle::after {
  content: '';
  position: absolute;
  top: 50%;
  left: 50%;
  width: 15%;
  height: 15%;
  background-color: #f8f9fa;
  transform: translate(-50%, -50%);
  border-radius: 50%;
}

.input-group .form-control[type='file'] {
  border-top-right-radius: 0;
  border-bottom-right-radius: 0;
}

.input-group .form-control:not([type='file']) {
  border-left: none;
  border-right: none;
  border-radius: 0;
}

.input-group .btn {
  border-top-left-radius: 0;
  border-bottom-left-radius: 0;
}

.btn:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

/* Drag state styles */
.form-control-enhanced[data-dragging='true'] {
  border-color: #0d6efd;
  background-color: #e7f1ff;
}

/* Responsive design */
@media (max-width: 768px) {
  .input-group {
    flex-direction: column;
  }

  .input-group .form-control,
  .input-group .btn {
    border-radius: 0.375rem !important;
    margin-bottom: 0.5rem;
  }

  .input-group .form-control:not(:last-child) {
    margin-bottom: 0.5rem;
  }

  .image-preview {
    max-width: 100%;
  }
}
</style>
