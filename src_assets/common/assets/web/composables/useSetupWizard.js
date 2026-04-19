import { ref } from 'vue'

/**
 * Setup wizard composable
 */
export function useSetupWizard() {
  const showSetupWizard = ref(false)
  const adapters = ref([])
  const displayDevices = ref([])
  const hasLocale = ref(false)

  // Check whether the setup wizard needs to be shown
  const checkSetupWizard = (config) => {
    const isFirstTime = config.setup_wizard_completed == true ||
                       config.setup_wizard_completed == 'true'

    if (!isFirstTime) {
      showSetupWizard.value = true
      adapters.value = config.adapters || []
      displayDevices.value = config.display_devices || []
      hasLocale.value = !!(config.locale && config.locale !== '')
      return true
    }
    return false
  }

  // Setup completion callback
  const onSetupComplete = (config) => {
    console.log('Setup complete:', config)
    // After the user clicks "Configure applications" they will be automatically redirected to /apps
  }

  return {
    showSetupWizard,
    adapters,
    displayDevices,
    hasLocale,
    checkSetupWizard,
    onSetupComplete,
  }
}

