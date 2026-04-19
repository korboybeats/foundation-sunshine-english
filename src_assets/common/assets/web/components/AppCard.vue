<template>
  <div class="app-card" :class="{ 'app-card-dragging': isDragging }">
    <div class="app-card-inner">
      <!-- App icon -->
      <div class="app-icon-container">
        <img 
          v-if="app['image-path']" 
          :src="getImageUrl()" 
          :alt="app.name"
          class="app-icon"
          @error="handleImageError"
        >
        <div v-else class="app-icon-placeholder">
          <i class="fas fa-desktop"></i>
        </div>
      </div>
      
      <!-- App info -->
      <div class="app-info" :title="app.cmd" @click="copyToClipboard(app.cmd, app.name, $event)">
        <h3 class="app-name">{{ app.name }}</h3>
        <p class="app-command" v-if="app.cmd">
          <i class="fas fa-terminal me-1"></i>
          {{ truncateText(app.cmd, 50) }}
        </p>
        <div class="app-tags">
          <span v-if="app['exclude-global-prep-cmd'] && app['exclude-global-prep-cmd'] !== 'false'" class="app-tag tag-exclude-global-prep-cmd">
            <i class="fas fa-ellipsis-h me-1"></i>Global prep cmd
          </span>
          <span v-if="app['menu-cmd'] && app['menu-cmd'].length > 0" class="app-tag tag-menu">
            <span class="badge rounded-pill bg-secondary me-1">{{ app['menu-cmd'].length }}</span>Menu commands
          </span>
          <span v-if="app.elevated && app.elevated !== 'false'" class="app-tag tag-elevated">
            <i class="fas fa-shield-alt me-1"></i>Admin
          </span>
          <span v-if="app['auto-detach'] && app['auto-detach'] !== 'false'" class="app-tag tag-detach">
            <i class="fas fa-unlink me-1"></i>Keep stream open on close
          </span>
        </div>
      </div>
      
      <!-- Action buttons -->
      <div class="app-actions">
        <button 
          class="btn btn-edit" 
          @click="$emit('edit')"
          :title="$t('apps.edit')"
        >
          <i class="fas fa-edit"></i>
        </button>
        <button 
          class="btn btn-delete" 
          @click="$emit('delete')"
          :title="$t('apps.delete')"
        >
          <i class="fas fa-trash"></i>
        </button>
      </div>
      
      <!-- Drag handle -->
      <div v-if="draggable" class="drag-handle">
        <i class="fas fa-grip-vertical"></i>
      </div>
      
      <!-- Search state indicator -->
      <div v-if="isSearchResult" class="search-indicator">
        <i class="fas fa-search"></i>
      </div>
    </div>
  </div>
</template>

<script>
import { getImagePreviewUrl } from '../utils/imageUtils.js';

export default {
  name: 'AppCard',
  props: {
    app: {
      type: Object,
      required: true
    },
    draggable: {
      type: Boolean,
      default: true
    },
    isSearchResult: {
      type: Boolean,
      default: false
    },
    isDragging: {
      type: Boolean,
      default: false
    }
  },
  emits: ['edit', 'delete', 'copy-success', 'copy-error'],
  methods: {
    /**
     * Handle image error
     */
    handleImageError(event) {
      const element = event.target;
      element.style.display = 'none';
      if (element.nextElementSibling) {
        element.nextElementSibling.style.display = 'flex';
      }
    },
    
    /**
     * Get the image URL
     */
    getImageUrl() {
      return getImagePreviewUrl(this.app['image-path']);
    },
    
    /**
     * Truncate text
     */
    truncateText(text, length) {
      if (!text) return '';
      if (text.length <= length) return text;
      return text.substring(0, length) + '...';
    },
    
    /**
     * Copy to clipboard
     */
    async copyToClipboard(text, appName, event) {
      if (!text) {
        this.$emit('copy-error', 'No command available to copy');
        return;
      }

      const targetElement = event.currentTarget;

      try {
        // Use the modern Clipboard API
        if (navigator.clipboard && window.isSecureContext) {
          await navigator.clipboard.writeText(text);
          this.showCopySuccess(targetElement, appName);
        } else {
          // Fallback: use the legacy execCommand
          const textArea = document.createElement('textarea');
          textArea.value = text;
          textArea.style.position = 'fixed';
          textArea.style.left = '-999999px';
          textArea.style.top = '-999999px';
          document.body.appendChild(textArea);
          textArea.focus();
          textArea.select();

          try {
            document.execCommand('copy');
            this.showCopySuccess(targetElement, appName);
          } catch (err) {
            console.error('Copy failed:', err);
            this.$emit('copy-error', 'Copy failed; please copy manually');
          } finally {
            document.body.removeChild(textArea);
          }
        }
      } catch (err) {
        console.error('Failed to copy to clipboard:', err);
        this.$emit('copy-error', 'Copy failed; please check browser permissions');
      }
    },

    /**
     * Show copy success animation and message
     */
    showCopySuccess(element, appName) {
      // Add the animation class
      element.classList.add('copy-success');

      // Emit the success event
      this.$emit('copy-success', `📋 Copied command for "${appName}"`);

      // Remove the animation class after 400ms
      setTimeout(() => {
        element.classList.remove('copy-success');
      }, 400);
    },
  }
}
</script> 