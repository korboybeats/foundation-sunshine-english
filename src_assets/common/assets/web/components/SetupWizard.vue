<template>
  <div class="setup-container">
    <div class="setup-card">
      <div class="setup-header">
        <img src="/images/logo-sunshine-256.png" height="60" alt="Sunshine">
        <h1>{{ $t('setup.welcome') }}</h1>
        <p>{{ $t('setup.description') }}</p>
      </div>

      <div class="setup-content">
        <!-- Step indicator -->
        <div class="step-indicator">
          <div class="step" :class="{ active: currentStep === 1, completed: currentStep > 1 }">
            <div class="step-number">1</div>
            <span>{{ $t('setup.step0_title') }}</span>
          </div>
          <div class="step-connector"></div>
          <div class="step" :class="{ active: currentStep === 2, completed: currentStep > 2 }">
            <div class="step-number">2</div>
            <span>{{ $t('setup.step2_title') }}</span>
          </div>
          <div class="step-connector"></div>
          <div class="step" :class="{ active: currentStep === 3, completed: currentStep > 3 }">
            <div class="step-number">3</div>
            <span>{{ $t('setup.step1_title') }}</span>
          </div>
          <div class="step-connector"></div>
          <div class="step" :class="{ active: currentStep === 4, completed: currentStep > 4 }">
            <div class="step-number">4</div>
            <span>{{ $t('setup.step3_title') }}</span>
          </div>
          <div class="step-connector"></div>
          <div class="step" :class="{ active: currentStep === 5 }">
            <div class="step-number">5</div>
            <span>{{ $t('setup.step4_title') }}</span>
          </div>
        </div>

        <!-- Step content -->
        <div class="step-content">
          <!-- Step 1: Choose language -->
          <div v-if="currentStep === 1">
            <h3 class="mb-4">{{ $t('setup.step0_description') }}</h3>
            
            <div class="option-card selected">
              <div class="option-icon">
                <i class="fas fa-language"></i>
              </div>
              <h4>English</h4>
              <p>English interface</p>
            </div>
          </div>

          <!-- Step 2: Select GPU -->
          <div v-else-if="currentStep === 2">
            <h3 class="mb-4">{{ $t('setup.step2_description') }}</h3>
            
            <div class="mb-3">
              <label for="adapterSelect" class="form-label adapter-label">{{ $t('setup.select_adapter') }}</label>
              <select id="adapterSelect" 
                      class="form-select form-select-large" 
                      v-model="selectedAdapter">
                <option value="">{{ $t('setup.choose_adapter') }}</option>
                <option v-for="adapter in uniqueAdapters" :key="adapter.name" :value="adapter.name">
                  {{ adapter.name }}
                </option>
              </select>
            </div>

              <div v-if="selectedAdapter" class="adapter-info">
                <h5>
                  <i class="fas fa-info-circle"></i>
                  {{ $t('setup.adapter_info') }}
                </h5>
                <p><strong>{{ $t('setup.selected_adapter') }}:</strong> {{ selectedAdapter }}</p>
              </div>

              <!-- GPU selection hint box -->
              <div class="form-text mt-3 adapter-hint-box" v-html="$t('config.adapter_name_desc_windows')"></div>
          </div>

          <!-- Step 3: Select streaming display -->
          <div v-else-if="currentStep === 3">
            <h3 class="mb-4">{{ $t('setup.step1_description') }}</h3>
            <p class="vdd-intro-text mb-4">{{ $t('setup.step1_vdd_intro') }}</p>
            
            <!-- Foundation display heading -->
            <h5 class="my-3 physical-display-title">
              <i class="fas fa-tv"></i>
              {{ $t('setup.base_display_title') }}
            </h5>
            <!-- Virtual display option -->
            <div class="option-card" 
                 :class="{ selected: selectedDisplay === 'ZakoHDR' }"
                 @click="selectedDisplay = 'ZakoHDR'">
              <div class="d-flex align-items-center">
                <div class="option-icon-small">
                  <i class="fas fa-tv"></i>
                </div>
                <div class="flex-grow-1">
                  <h4>{{ $t('setup.virtual_display') }}</h4>
                  <p>{{ $t('setup.virtual_display_desc') }}</p>
                </div>
              </div>
            </div>

            <!-- Physical display list -->
            <div v-if="displayDevices && displayDevices.length > 0">
              <h5 class="my-3 physical-display-title">
                <i class="fas fa-desktop"></i>
                {{ $t('setup.physical_display') }}
              </h5>
              <div class="option-card" 
                   v-for="device in displayDevices" 
                   :key="device.device_id"
                   :class="{ selected: selectedDisplay === device.device_id }"
                   @click="selectedDisplay = device.device_id">
                <div class="d-flex align-items-center">
                  <div class="option-icon-small">
                    <i class="fas fa-desktop"></i>
                  </div>
                  <div class="flex-grow-1">
                    <h4>{{ getDisplayName(device) }}</h4>
                    <p>{{ getDisplayInfo(device) }}</p>
                  </div>
                </div>
              </div>
            </div>
          </div>

          <!-- Step 4: Select display combination strategy -->
          <div v-else-if="currentStep === 4">
            <h3 class="mb-4">{{ $t('setup.step3_description') }}</h3>

            <!-- Display combination strategy (unified for VDD and physical modes) -->
              <div class="option-card-compact"
                   :class="{ selected: displayDevicePrep === 'ensure_only_display' }"
                   @click="displayDevicePrep = 'ensure_only_display'">
                <div class="option-icon-compact"><i class="fas fa-desktop"></i></div>
                <div class="option-text">
                  <h4>{{ $t('setup.step3_ensure_only_display') }}</h4>
                  <p>{{ $t('setup.step3_ensure_only_display_desc') }}</p>
                </div>
              </div>

              <div class="option-card-compact" 
                   :class="{ selected: displayDevicePrep === 'ensure_primary' }"
                   @click="displayDevicePrep = 'ensure_primary'">
                <div class="option-icon-compact"><i class="fas fa-star"></i></div>
                <div class="option-text">
                  <h4>{{ $t('setup.step3_ensure_primary') }}</h4>
                  <p>{{ $t('setup.step3_ensure_primary_desc') }}</p>
                </div>
              </div>

              <div class="option-card-compact" 
                   :class="{ selected: displayDevicePrep === 'ensure_active' }"
                   @click="displayDevicePrep = 'ensure_active'">
                <div class="option-icon-compact"><i class="fas fa-check-circle"></i></div>
                <div class="option-text">
                  <h4>{{ $t('setup.step3_ensure_active') }}</h4>
                  <p>{{ $t('setup.step3_ensure_active_desc') }}</p>
                </div>
              </div>

              <div class="option-card-compact" 
                   :class="{ selected: displayDevicePrep === 'ensure_secondary' }"
                   @click="displayDevicePrep = 'ensure_secondary'">
                <div class="option-icon-compact"><i class="fas fa-columns"></i></div>
                <div class="option-text">
                  <h4>{{ $t('setup.step3_ensure_secondary') }}</h4>
                  <p>{{ $t('setup.step3_ensure_secondary_desc') }}</p>
                </div>
              </div>

              <div class="option-card-compact" 
                   :class="{ selected: displayDevicePrep === 'no_operation' }"
                   @click="displayDevicePrep = 'no_operation'">
                <div class="option-icon-compact"><i class="fas fa-hand-paper"></i></div>
                <div class="option-text">
                  <h4>{{ $t('setup.step3_no_operation') }}</h4>
                  <p>{{ $t('setup.step3_no_operation_desc') }}</p>
                </div>
              </div>
          </div>

          <!-- Step 5: Done -->
          <div v-else-if="currentStep === 5">
            <div>
              <div class="text-center mb-3">
                <h3 class="mb-1">
                  <i class="fas fa-check-circle setup-complete-icon"></i>
                  {{ $t('setup.setup_complete') }}
                </h3>
                <p class="mb-0">{{ $t('setup.setup_complete_desc') }}</p>
              </div>
              
              <div class="alert alert-info text-center" v-if="saveSuccess">
                <i class="fas fa-info-circle"></i>
                {{ $t('setup.config_saved') }}
              </div>
              
              <div class="alert alert-danger" v-if="saveError">
                <i class="fas fa-exclamation-triangle"></i>
                {{ saveError }}
              </div>

              <!-- Client downloads -->
              <div class="client-download-section mt-3">
                <h5 class="mb-3">
                  <i class="fas fa-download"></i>
                  {{ $t('setup.download_clients') }}
                </h5>
                <div class="client-download-layout">
                  <!-- Left: app download links -->
                  <div class="client-links">
                    <a class="resource-link resource-link-android"
                       href="https://github.com/qiin2333/moonlight-vplus"
                       target="_blank">
                      <div class="resource-icon"><i class="fab fa-android"></i></div>
                      <div class="resource-content">
                        <span class="resource-title">Android Moonlight V+</span>
                        <span class="resource-desc">Android / Android TV</span>
                      </div>
                      <i class="fas fa-external-link-alt resource-arrow"></i>
                    </a>
                    <a class="resource-link resource-link-harmony"
                       href="javascript:void(0)"
                       @click.prevent="openHarmonyModal">
                      <div class="resource-icon"><i class="fas fa-mobile-alt"></i></div>
                      <div class="resource-content">
                        <span class="resource-title">HarmonyOS Moonlight V+</span>
                        <span class="resource-desc">HarmonyOS NEXT</span>
                      </div>
                      <i class="fas fa-external-link-alt resource-arrow"></i>
                    </a>
                    <a class="resource-link resource-link-apple"
                       href="https://apps.apple.com/cn/app/voidlink/id6747717070"
                       target="_blank">
                      <div class="resource-icon"><i class="fab fa-apple"></i></div>
                      <div class="resource-content">
                        <span class="resource-title">VoidLink</span>
                        <span class="resource-desc">iOS / iPadOS</span>
                      </div>
                      <i class="fas fa-external-link-alt resource-arrow"></i>
                    </a>
                    <a class="resource-link resource-link-desktop"
                       href="https://github.com/qiin2333/moonlight-qt"
                       target="_blank">
                      <div class="resource-icon"><i class="fas fa-desktop"></i></div>
                      <div class="resource-content">
                        <span class="resource-title">Moonlight PC</span>
                        <span class="resource-desc">Windows / macOS / Linux</span>
                      </div>
                      <i class="fas fa-external-link-alt resource-arrow"></i>
                    </a>
                  </div>
                  <!-- Right: QR codes -->
                  <div class="client-qrcodes">
                    <div class="qr-code-item">
                      <div class="qr-code-box">
                        <img :src="androidQrCode" alt="Android QR Code" class="qr-code-image">
                      </div>
                      <div class="qr-code-label">
                        <i class="fab fa-android"></i>
                        {{ $t('setup.android_client') }}
                      </div>
                    </div>
                    <div class="qr-code-item">
                      <div class="qr-code-box">
                        <img :src="iosQrCode" alt="iOS QR Code" class="qr-code-image">
                      </div>
                      <div class="qr-code-label">
                        <i class="fab fa-apple"></i>
                        {{ $t('setup.ios_client') }}
                      </div>
                    </div>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </div>

      </div>

      <!-- Action buttons (sticky footer) -->
      <div class="action-buttons">
          <button class="btn btn-setup btn-setup-secondary" 
                  @click="previousStep" 
                  v-if="currentStep > 1 && currentStep < 5"
                  :disabled="saving">
            <i class="fas fa-arrow-left"></i>
            {{ $t('setup.previous') }}
          </button>
          <button class="btn btn-setup btn-setup-skip" 
                  @click="skipWizard" 
                  v-if="currentStep < 5"
                  :disabled="saving"
                  type="button">
            <i class="fas fa-forward"></i>
            {{ $t('setup.skip') }}
          </button>
          <div v-else></div>

          <button class="btn btn-setup btn-setup-primary" 
                  @click="nextStep" 
                  v-if="currentStep < 5"
                  :disabled="!canProceed || saving">
            {{ currentStep === 4 ? $t('setup.finish') : $t('setup.next') }}
            <i class="fas fa-arrow-right"></i>
          </button>

          <button class="btn btn-setup btn-setup-primary" 
                  @click="goToApps" 
                  v-if="currentStep === 5">
            {{ $t('setup.go_to_apps') }}
            <i class="fas fa-arrow-right"></i>
          </button>
      </div>
    </div>
    <!-- Skip Wizard Modal -->
    <Transition name="fade">
      <div v-if="showSkipModal" class="skip-wizard-overlay" @click.self="closeSkipModal">
        <div class="skip-wizard-modal">
          <div class="skip-wizard-header">
            <h5>{{ $t('setup.skip_confirm_title')}}</h5>
            <button class="btn-close" @click="closeSkipModal"></button>
          </div>
          <div class="skip-wizard-body">
            <p>{{ $t('setup.skip_confirm') }}</p>
          </div>
          <div class="skip-wizard-footer">
            <button type="button" class="btn btn-secondary" @click="closeSkipModal">{{ $t('_common.cancel') }}</button>
            <button type="button" class="btn btn-warning" @click="confirmSkipWizard">{{ $t('setup.skip') }}</button>
          </div>
        </div>
      </div>
    </Transition>

    <!-- Harmony Link Modal -->
    <Teleport to="body">
    <Transition name="fade">
      <div v-if="showHarmonyModal" class="skip-wizard-overlay" @click.self="closeHarmonyModal">
        <div class="skip-wizard-modal">
          <div class="skip-wizard-header">
            <h5>HarmonyOS Moonlight V+</h5>
            <button class="btn-close" @click="closeHarmonyModal"></button>
          </div>
          <div class="skip-wizard-body">
            <p>{{ $t('setup.harmony_modal_link_notice') }}</p>
            <p>{{ $t('setup.harmony_modal_desc') }}</p>
          </div>
          <div class="skip-wizard-footer">
            <button type="button" class="btn btn-secondary" @click="closeHarmonyModal">{{ $t('_common.cancel') }}</button>
            <button type="button" class="btn btn-primary" @click="confirmHarmonyLink">
              <i class="fas fa-external-link-alt me-1"></i>
              {{ $t('setup.harmony_goto_repo') }}
            </button>
          </div>
        </div>
      </div>
    </Transition>
    </Teleport>

    <!-- Restart Countdown Modal -->
    <Teleport to="body">
    <Transition name="fade">
      <div v-if="showRestartModal" class="skip-wizard-overlay">
        <div class="skip-wizard-modal">
          <div class="skip-wizard-header">
            <h5><i class="fas fa-sync-alt me-2"></i>{{ $t('setup.restart_title') }}</h5>
          </div>
          <div class="skip-wizard-body text-center">
            <p>{{ $t('setup.restart_desc') }}</p>
            <div class="restart-countdown my-3">
              <span class="display-4 fw-bold text-primary">{{ restartCountdown }}</span>
              <p class="text-muted mt-1">{{ $t('setup.restart_countdown_unit') }}</p>
            </div>
            <div class="progress" style="height: 6px;">
              <div class="progress-bar bg-primary" :style="{ width: (restartCountdown / 8 * 100) + '%' }" role="progressbar"></div>
            </div>
          </div>
          <div class="skip-wizard-footer">
            <button type="button" class="btn btn-primary" @click="skipRestartCountdown">
              <i class="fas fa-arrow-right me-1"></i>
              {{ $t('setup.restart_go_now') }}
            </button>
          </div>
        </div>
      </div>
    </Transition>
    </Teleport>
  </div>
</template>

<script>
import { trackEvents } from '../config/firebase.js'
import { openExternalUrl } from '../utils/helpers.js'

export default {
  name: 'SetupWizard',
  props: {
    adapters: {
      type: Array,
      default: () => []
    },
    displayDevices: {
      type: Array,
      default: () => []
    },
    hasLocale: {
      type: Boolean,
      default: false
    }
  },
  data() {
    return {
      currentStep: 1,
      selectedLocale: 'zh', // Default to Simplified Chinese
      selectedDisplay: 'ZakoHDR', // Default to the Foundation virtual display
      selectedAdapter: '',
      displayDevicePrep: 'ensure_only_display', // Default: ensure single display (works for both VDD and physical modes)
      saveSuccess: false,
      saveError: null,
      saving: false,
      showSkipModal: false, // Skip-wizard confirmation modal
      showHarmonyModal: false, // HarmonyOS link reminder modal
      showRestartModal: false, // Restart countdown modal
      restartCountdown: 8, // Countdown seconds
      restartTimer: null, // Countdown timer
      // Client download links
      androidQrCode: 'https://assets.alkaidlab.com/androidQrCode.png',
      iosQrCode: 'https://assets.alkaidlab.com/iosQrCode.png',
    }
  },
  setup() {
    return {}
  },
  mounted() {
    // Record entry into the setup wizard
    trackEvents.pageView('setup_wizard')
    trackEvents.userAction('setup_wizard_started', {
      has_locale: this.hasLocale,
      adapter_count: this.adapters.length
    })
    
    // If a language is already configured, skip the first step
    if (this.hasLocale) {
      this.currentStep = 2
      trackEvents.userAction('setup_wizard_skip_language', { 
        reason: 'already_configured' 
      })
    }
    
    // If there is only one GPU, select it automatically
    if (this.uniqueAdapters.length === 1) {
      this.selectedAdapter = this.uniqueAdapters[0].name
    }
  },
  beforeUnmount() {
    if (this.restartTimer) {
      clearInterval(this.restartTimer)
      this.restartTimer = null
    }
  },
  computed: {
    canProceed() {
      if (this.currentStep === 1) {
        return this.selectedLocale !== null
      } else if (this.currentStep === 2) {
        return this.selectedAdapter !== null
      } else if (this.currentStep === 3) {
        return this.selectedDisplay !== null
      } else if (this.currentStep === 4) {
        return this.displayDevicePrep !== null
      }
      return false
    },
    isVirtualDisplay() {
      return this.selectedDisplay === 'ZakoHDR'
    },
    // Deduplicate by name, keeping the first occurrence
    uniqueAdapters() {
      const list = this.adapters ?? []
      const seen = new Set()
      return list.filter((a) => {
        const name = a?.name ?? ''
        if (seen.has(name)) return false
        seen.add(name)
        return true
      })
    }
  },
  methods: {
    previousStep() {
      if (this.currentStep > 1) {
        this.currentStep--
      }
    },
    async nextStep() {
      if (this.currentStep === 1 && this.canProceed) {
        // Save the language setting and reload
        await this.saveLanguage()
      } else if (this.currentStep === 2 && this.canProceed) {
        this.currentStep++
      } else if (this.currentStep === 3 && this.canProceed) {
        this.currentStep++
      } else if (this.currentStep === 4 && this.canProceed) {
        await this.saveConfiguration()
      }
    },
    async saveLanguage() {
      try {
        await fetch('/api/config', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify({
            locale: this.selectedLocale
          }),
        })
        // Reload the page to apply the new language
        window.location.reload()
      } catch (error) {
        console.error('Failed to save language:', error)
      }
    },
    async saveConfiguration() {
      this.saving = true
      this.saveError = null

      try {
        // First fetch the full current configuration to preserve all existing settings
        const currentConfig = await fetch('/api/config').then(r => r.json())

        // Copy every field from the full config to avoid overwriting other settings
        const config = { ...currentConfig }

        // Mark the setup wizard as completed
        config.setup_wizard_completed = true

        // Ensure locale is saved (if the user picked a language in step 1, or it already exists in the config)
        if (this.selectedLocale) {
          config.locale = this.selectedLocale
        } else if (currentConfig.locale) {
          config.locale = currentConfig.locale
        }

        // Set adapter_name
        config.adapter_name = this.selectedAdapter || ''

        // Set the selected display
        config.output_name = this.selectedDisplay

        // Save display_device_prep uniformly (works for both VDD and physical modes)
        config.display_device_prep = this.displayDevicePrep

        console.log('Saving configuration:', config)

        const response = await fetch('/api/config', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify(config),
        })

        if (response.ok) {
          this.saveSuccess = true
          this.currentStep = 5
          
          // Record setup completion
          trackEvents.userAction('setup_wizard_completed', {
            selected_display: this.selectedDisplay,
            adapter: this.selectedAdapter,
            display_device_prep: this.displayDevicePrep,
            is_virtual_display: this.isVirtualDisplay
          })
          
          this.$emit('setup-complete', config)
        } else {
          const errorText = await response.text()
          this.saveError = `${this.$t('setup.save_error')}: ${errorText}`
          
          // Record save failure
          trackEvents.errorOccurred('setup_config_save_failed', errorText)
        }
      } catch (error) {
        console.error('Failed to save configuration:', error)
        this.saveError = `${this.$t('setup.save_error')}: ${error.message}`
      } finally {
        this.saving = false
      }
    },
    skipWizard(event) {
      if (event) {
        event.preventDefault()
        event.stopPropagation()
      }
      
      if (this.saving) return
      
      this.openSkipModal()
    },
    openSkipModal() {
      this.showSkipModal = true
    },
    closeSkipModal() {
      this.showSkipModal = false
    },
    openHarmonyModal() {
      this.showHarmonyModal = true
    },
    closeHarmonyModal() {
      this.showHarmonyModal = false
    },
    async confirmHarmonyLink() {
      this.closeHarmonyModal()
      try {
        await openExternalUrl('https://github.com/AlkaidLab/moonlight-harmony')
      } catch (error) {
        console.error('Failed to open URL:', error)
      }
    },
    async confirmSkipWizard() {
      // Close the modal
      this.closeSkipModal()
      
      if (this.saving) return

      this.saving = true
      this.saveError = null

      try {
        // First fetch the full current configuration to preserve all existing settings
        const currentConfig = await fetch('/api/config').then(r => r.json())

        // Copy every field from the full config to avoid overwriting other settings
        const config = { ...currentConfig }
        // Mark the setup wizard as completed
        config.setup_wizard_completed = true
        console.log('Skipping setup wizard, saving config:', config)
        const response = await fetch('/api/config', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify(config),
        })

        if (response.ok) {
          // Record the skip event
          trackEvents.userAction('setup_wizard_skipped', {
            from_step: this.currentStep
          })
          
          // Emit the completion event so the parent knows the wizard finished
          this.$emit('setup-complete', config)
          
          // Reload the page to hide the setup wizard
          window.location.reload()
        } else {
          const errorText = await response.text()
          this.saveError = `${this.$t('setup.skip_error')}: ${errorText}`
          
          // Record the skip failure
          trackEvents.errorOccurred('setup_wizard_skip_failed', errorText)
        }
      } catch (error) {
        console.error('Failed to skip wizard:', error)
        this.saveError = `${this.$t('setup.skip_error')}: ${error.message}`
      } finally {
        this.saving = false
      }
    },
    goToApps() {
      // Record navigation to the apps page
      trackEvents.userAction('setup_go_to_apps', {
        from_step: this.currentStep
      })
      // Trigger restart and show the countdown
      this.triggerRestartAndRedirect()
    },
    async triggerRestartAndRedirect() {
      // Call the restart API
      try {
        await fetch('/api/restart', { method: 'POST' })
      } catch {
        // The restart request may drop the connection; ignore the error
      }
      // Show the countdown modal
      this.showRestartModal = true
      this.restartCountdown = 8
      this.restartTimer = setInterval(() => {
        this.restartCountdown--
        if (this.restartCountdown <= 0) {
          this.finishRedirect()
        }
      }, 1000)
    },
    skipRestartCountdown() {
      this.finishRedirect()
    },
    finishRedirect() {
      if (this.restartTimer) {
        clearInterval(this.restartTimer)
        this.restartTimer = null
      }
      this.showRestartModal = false
      window.location.href = '/'
    },
    getDisplayName(device) {
      // Parse device.data and extract the friendly name
      // Data format:
      // DISPLAY NAME: \\.\\DISPLAY1
      // FRIENDLY NAME: F32D80U
      // DEVICE STATE: PRIMARY
      // HDR STATE: ENABLED
      try {
        const data = device.data || ''
        const name = data
          .replace(
            /.*?(DISPLAY\d+)?\nFRIENDLY NAME: (.*[^\n])*?\n.*\n.*/g,
            "$2 ($1)"
          )
          .replace("()", "")
        
        return name || device.device_id || this.$t('setup.unknown_display')
      } catch (e) {
        return device.device_id || this.$t('setup.unknown_display')
      }
    },
    getDisplayInfo(device) {
      // Parse device.data and extract detailed info
      try {
        const data = device.data || ''
        
        // Extract DEVICE STATE
        const stateMatch = data.match(/DEVICE STATE: (\w+)/)
        const state = stateMatch ? stateMatch[1].toLowerCase() : 'unknown'
        
        // Extract HDR STATE
        const hdrMatch = data.match(/HDR STATE: (\w+)/)
        const hdr = hdrMatch ? hdrMatch[1] : ''
        
        const stateKey = {
          'primary': 'setup.state_primary',
          'active': 'setup.state_active',
          'inactive': 'setup.state_inactive'
        }[state] || 'setup.state_unknown'
        const stateText = this.$t(stateKey)
        
        let info = `${this.$t('setup.device_state')}: ${stateText}`
        if (hdr) {
          info += ` | HDR: ${hdr}`
        }
        
        return info
      } catch (e) {
        return device.device_id
      }
    }
  }
}
</script>

<style scoped>
.setup-container {
  position: fixed;
  inset: 0;
  padding: 1em;
  display: flex;
  flex-direction: column;
  align-items: center;
  z-index: 1000;
}

.setup-card {
  background: var(--bs-body-bg);
  border-radius: 16px;
  box-shadow: 0 4px 20px rgba(0, 0, 0, 0.1);
  overflow: hidden;
  display: flex;
  flex-direction: column;
  width: 100%;
  max-width: 900px;
  flex: 1;
  min-height: 0;
}

.setup-header {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  color: white;
  padding: 1.2em;
  text-align: center;
  flex-shrink: 0;
}

.setup-header h1 {
  margin: 0.3em 0 0 0;
  font-size: 1.5em;
  font-weight: 600;
}

.setup-header p {
  margin: 0.3em 0 0 0;
  opacity: 0.9;
  font-size: 0.95em;
}

.setup-content {
  padding: 1.2em 1.5em;
  display: flex;
  flex-direction: column;
  flex: 1;
  overflow: hidden;
  min-height: 0;
}

.step-indicator {
  display: flex;
  justify-content: center;
  align-items: center;
  margin-bottom: 1.2em;
  gap: 0.5em;
  flex-shrink: 0;
}

.step {
  display: flex;
  align-items: center;
  gap: 0.3em;
  font-size: 0.85em;
}

.step-number {
  width: 28px;
  height: 28px;
  border-radius: 50%;
  display: flex;
  align-items: center;
  justify-content: center;
  font-weight: 600;
  font-size: 0.9em;
  background: var(--bs-secondary-bg);
  color: var(--bs-secondary-color);
  transition: all 0.3s ease;
  flex-shrink: 0;
}

.step.active .step-number {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  color: white;
  transform: scale(1.05);
}

.step.completed .step-number {
  background: #28a745;
  color: white;
}

.step-connector {
  width: 30px;
  height: 2px;
  background: var(--bs-secondary-bg);
  flex-shrink: 0;
}

.step-content {
  flex: 1;
  overflow-y: auto;
  overflow-x: hidden;
  padding-right: 0.5em;
}

.step-content h3 {
  font-size: 1.1em;
  margin-bottom: 0.8em;
}

.option-card {
  border: 2px solid var(--bs-border-color);
  border-radius: 10px;
  padding: 0.8em 1em;
  margin-bottom: 0.6em;
  cursor: pointer;
  transition: all 0.3s ease;
  background: var(--bs-body-bg);
}

.option-card:hover {
  border-color: #667eea;
  transform: translateY(-1px);
  box-shadow: 0 3px 10px rgba(102, 126, 234, 0.2);
}

.option-card.selected {
  border-color: #667eea;
  background: rgba(102, 126, 234, 0.1);
}

.option-card .option-icon {
  font-size: 1.8em;
  margin-bottom: 0.3em;
  color: #667eea;
}

.option-card h4 {
  margin: 0.3em 0;
  font-weight: 600;
  font-size: 1em;
}

.option-card p {
  margin: 0;
  color: var(--bs-body-color);
  opacity: 0.85;
  font-size: 0.85em;
  line-height: 1.3;
}

/* Compact option card (horizontal layout) */
.option-card-compact {
  display: flex;
  align-items: center;
  border: 2px solid var(--bs-border-color);
  border-radius: 8px;
  padding: 0.5em 0.8em;
  margin-bottom: 0.4em;
  cursor: pointer;
  transition: all 0.3s ease;
  background: var(--bs-body-bg);
  gap: 0.7em;
}

.option-card-compact:hover {
  border-color: #667eea;
  transform: translateY(-1px);
  box-shadow: 0 2px 8px rgba(102, 126, 234, 0.2);
}

.option-card-compact.selected {
  border-color: #667eea;
  background: rgba(102, 126, 234, 0.1);
}

.option-icon-compact {
  font-size: 1.2em;
  color: #667eea;
  flex-shrink: 0;
  width: 2em;
  text-align: center;
}

.option-card-compact .option-text h4 {
  margin: 0;
  font-weight: 600;
  font-size: 0.9em;
}

.option-card-compact .option-text p {
  margin: 0;
  color: var(--bs-body-color);
  opacity: 0.75;
  font-size: 0.85em;
  line-height: 1.3;
}

.form-select-large {
  padding: 0.7em;
  font-size: 1em;
  border-radius: 8px;
  border: 2px solid var(--bs-border-color);
  transition: all 0.3s ease;
}

/* Adapter label */
.adapter-label {
  font-size: 1.05em;
  font-weight: 600;
}

/* Physical display heading */
.physical-display-title {
  font-size: 0.95em;
}

.form-select-large:focus {
  border-color: #667eea;
  box-shadow: 0 0 0 0.2rem rgba(102, 126, 234, 0.25);
}

.action-buttons {
  display: flex;
  justify-content: space-between;
  gap: 0.8em;
  flex-shrink: 0;
  padding: 1em 1.5em;
  border-top: 1px solid var(--bs-border-color);
  background: var(--bs-body-bg);
}

.btn-setup {
  padding: 0.6em 1.5em;
  font-size: 1em;
  border-radius: 8px;
  font-weight: 500;
  transition: all 0.3s ease;
}

.btn-setup-primary {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  border: none;
  color: white;
}

.btn-setup-primary:hover:not(:disabled) {
  transform: translateY(-2px);
  box-shadow: 0 4px 12px rgba(102, 126, 234, 0.4);
  color: white;
}

.btn-setup-secondary {
  background: var(--bs-secondary-bg);
  border: none;
  color: var(--bs-body-color);
}

.btn-setup-secondary:hover:not(:disabled) {
  background: var(--bs-tertiary-bg);
  transform: translateY(-1px);
}

.btn-setup-skip {
  background: rgba(255, 255, 255, 0.95);
  border: 2px solid rgba(102, 126, 234, 0.7);
  color: rgba(70, 90, 200, 1);
  font-weight: 500;
}

.btn-setup-skip:hover:not(:disabled) {
  background: rgba(255, 255, 255, 1);
  border-color: rgba(102, 126, 234, 0.9);
  color: rgba(50, 70, 180, 1);
  transform: translateY(-1px);
  box-shadow: 0 2px 8px rgba(102, 126, 234, 0.3);
}

.adapter-info {
  background: var(--bs-secondary-bg);
  padding: 0.8em;
  border-radius: 8px;
  margin-top: 0.8em;
  font-size: 0.9em;
}

.adapter-info h5 {
  font-size: 1em;
  margin-bottom: 0.5em;
}

.adapter-info p {
  margin-bottom: 0.3em;
  font-size: 0.95em;
}

/* GPU selection hint box styles */
.adapter-hint-box {
  background: rgba(102, 126, 234, 0.08);
  padding: 0.8em 1em;
  border-radius: 8px;
  border-left: 3px solid #667eea;
  font-size: 0.9em;
  line-height: 1.5;
  color: var(--bs-body-color);
  font-weight: 500;
}

/* VDD intro text styles */
.vdd-intro-text {
  color: var(--bs-body-color);
  opacity: 0.75;
  font-size: 0.95em;
}

.adapter-vdd-hint {
  margin: 0.5em 0 0 0;
  padding: 0.5em 0.8em;
  background: rgba(40, 167, 69, 0.1);
  border-radius: 4px;
  font-size: 0.95em;
  white-space: pre-wrap;
  word-wrap: break-word;
}

/* Scrollbar styles */
.step-content::-webkit-scrollbar {
  width: 6px;
}

.step-content::-webkit-scrollbar-track {
  background: transparent;
}

.step-content::-webkit-scrollbar-thumb {
  background: rgba(102, 126, 234, 0.3);
  border-radius: 3px;
}

.step-content::-webkit-scrollbar-thumb:hover {
  background: rgba(102, 126, 234, 0.5);
}

/* Completion page heading */
.setup-complete-icon {
  font-size: 1.2em;
  color: #28a745;
  margin-right: 0.3em;
  vertical-align: middle;
}

/* Client downloads styles */
.client-download-section {
  background: var(--bs-secondary-bg);
  padding: 1em;
  border-radius: 10px;
}

.client-download-section h5 {
  font-size: 1em;
  margin-bottom: 0.8em;
  color: var(--bs-body-color);
}

.client-download-layout {
  display: flex;
  gap: 1.5em;
  align-items: flex-start;
}

.client-links {
  flex: 0 0 auto;
  display: flex;
  flex-direction: column;
  gap: 0.5em;
  min-width: 240px;
}

.client-qrcodes {
  display: flex;
  flex-direction: row;
  gap: 1em;
  align-items: flex-start;
  flex: 1;
  min-width: 0;
}

/* Resource link styles (from ResourceCard) */
.resource-link {
  display: flex;
  align-items: center;
  padding: 0.6em 0.8em;
  border-radius: 8px;
  text-decoration: none;
  background: linear-gradient(135deg, rgba(var(--link-color), 0.15) 0%, rgba(var(--link-color), 0.08) 100%);
  border: 1px solid transparent;
  transition: transform 0.2s ease, box-shadow 0.2s ease, border-color 0.2s ease;
}

.resource-link:hover {
  transform: translateY(-1px);
  box-shadow: 0 3px 10px rgba(0, 0, 0, 0.12);
  text-decoration: none;
  border-color: rgba(var(--link-color), 0.4);
}

.resource-icon {
  width: 36px;
  height: 36px;
  border-radius: 8px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 1.1rem;
  flex-shrink: 0;
  margin-right: 0.8em;
  color: white;
  background: var(--icon-gradient);
}

.resource-content {
  flex: 1;
  min-width: 0;
}

.resource-title {
  display: block;
  font-weight: 600;
  font-size: 0.9rem;
  color: var(--bs-body-color);
  margin-bottom: 1px;
}

.resource-desc {
  display: block;
  font-size: 0.75rem;
  color: var(--bs-secondary-color);
}

.resource-arrow {
  font-size: 0.8rem;
  color: var(--bs-secondary-color);
  margin-left: 0.5rem;
  transition: transform 0.2s ease;
}

.resource-link:hover .resource-arrow {
  transform: translateX(3px);
}

.resource-link-android {
  --link-color: 61, 220, 132;
  --icon-gradient: linear-gradient(135deg, #3ddc84 0%, #00c853 100%);
}

.resource-link-apple {
  --link-color: 128, 128, 128;
  --icon-gradient: linear-gradient(135deg, #555 0%, #777 100%);
}

.resource-link-desktop {
  --link-color: 108, 117, 125;
  --icon-gradient: linear-gradient(135deg, #6c757d 0%, #495057 100%);
}

.resource-link-harmony {
  --link-color: 206, 48, 48;
  --icon-gradient: linear-gradient(135deg, #ce3030 0%, #e74c3c 100%);
}

.qr-code-item {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 0.3em;
  flex: 1;
}

.qr-code-box {
  background: white;
  padding: 0.4em;
  border-radius: 8px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
  width: 100%;
}

.qr-code-image {
  width: 100%;
  aspect-ratio: 1;
  display: block;
}

.qr-code-label {
  font-size: 0.8em;
  font-weight: 500;
  color: var(--bs-body-color);
}

.qr-code-label i {
  margin-right: 0.3em;
}

/* Small icon styles */
.option-icon-small {
  font-size: 1.5em;
  color: #667eea;
  margin-right: 0.8em;
  flex-shrink: 0;
}

.d-flex {
  display: flex;
}

.align-items-center {
  align-items: center;
}

.flex-grow-1 {
  flex-grow: 1;
}

.my-3 {
  margin-top: 1rem;
  margin-bottom: 1rem;
}

/* Skip Wizard Modal - reuses ScanResultModal styles */
.skip-wizard-overlay {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  width: 100vw;
  height: 100vh;
  margin: 0;
  background: var(--overlay-bg, rgba(0, 0, 0, 0.7));
  backdrop-filter: blur(8px);
  z-index: 9999;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: var(--spacing-lg, 20px);
  overflow: hidden;
  
  [data-bs-theme='light'] & {
    background: rgba(0, 0, 0, 0.5);
  }
}

.skip-wizard-modal {
  background: var(--modal-bg, rgba(30, 30, 50, 0.95));
  border: 1px solid var(--border-color-light, rgba(255, 255, 255, 0.2));
  border-radius: var(--border-radius-xl, 12px);
  width: 100%;
  max-width: 500px;
  max-height: 80vh;
  display: flex;
  flex-direction: column;
  backdrop-filter: blur(20px);
  box-shadow: var(--shadow-xl, 0 25px 50px rgba(0, 0, 0, 0.5));
  animation: modalSlideUp 0.3s ease;
  
  [data-bs-theme='light'] & {
    background: rgba(255, 255, 255, 0.95);
    border: 1px solid rgba(0, 0, 0, 0.15);
    box-shadow: 0 25px 50px rgba(0, 0, 0, 0.2);
  }
}

@keyframes modalSlideUp {
  from {
    transform: translateY(20px);
    opacity: 0;
  }
  to {
    transform: translateY(0);
    opacity: 1;
  }
}

.skip-wizard-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: var(--spacing-md, 20px) var(--spacing-lg, 24px);
  border-bottom: 1px solid var(--border-color-light, rgba(255, 255, 255, 0.1));

  h5 {
    margin: 0;
    color: var(--text-primary, #fff);
    font-size: var(--font-size-lg, 1.1rem);
    font-weight: 600;
    display: flex;
    align-items: center;
    gap: var(--spacing-sm, 8px);
  }
  
  [data-bs-theme='light'] & {
    border-bottom: 1px solid rgba(0, 0, 0, 0.1);
    
    h5 {
      color: #000000;
    }
  }
}

.skip-wizard-body {
  padding: var(--spacing-lg, 24px);
  font-size: var(--font-size-md, 0.95rem);
  line-height: 1.5;
  overflow-y: auto;
  flex: 1;
  color: var(--text-primary, #fff);
  
  [data-bs-theme='light'] & {
    color: #000000;
  }
}

.skip-wizard-footer {
  display: flex;
  justify-content: flex-end;
  gap: 10px;
  padding: var(--spacing-md, 20px) var(--spacing-lg, 24px);
  border-top: 1px solid var(--border-color-light, rgba(255, 255, 255, 0.1));
  
  [data-bs-theme='light'] & {
    border-top: 1px solid rgba(0, 0, 0, 0.1);
  }
}

.skip-wizard-footer button {
  padding: 8px 16px;
  font-size: 0.9rem;
}

/* Vue transition animations */
.fade-enter-active {
  transition: opacity 0.3s ease;
}

.fade-leave-active {
  transition: opacity 0.3s ease;
}

.fade-enter-from,
.fade-leave-to {
  opacity: 0;
}
</style>

