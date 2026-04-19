#!/usr/bin/env node
/**
 * Generate the complete welcome.html from the template file.
 * Extracts the welcome translations for every language and embeds them statically into the HTML.
 * Note: only the welcome section is extracted because the _common keys we need have already been
 * merged into welcome.
 */

import fs from 'fs'
import path from 'path'
import { fileURLToPath } from 'url'

const __filename = fileURLToPath(import.meta.url)
const __dirname = path.dirname(__filename)

const localeDir = path.join(__dirname, '../public/assets/locale')
const templatePath = path.join(__dirname, '../welcome.html.template')
const outputPath = path.join(__dirname, '../welcome.html')
const output = {}

// Get all locale files
const localeFiles = fs.readdirSync(localeDir).filter(file => file.endsWith('.json'))

localeFiles.forEach(file => {
  const locale = file.replace('.json', '')
  const filePath = path.join(localeDir, file)
  
  try {
    const content = JSON.parse(fs.readFileSync(filePath, 'utf8'))
    // Only extract the welcome section (username, password, error, success are already inside welcome)
    if (content.welcome) {
      output[locale] = {
        welcome: content.welcome
      }
    }
  } catch (e) {
    console.error(`Failed to parse ${file}:`, e.message)
  }
})

// Build the inline <script> tag contents
const inlineScript = `<script>
// Auto-generated welcome-page translation data (produced at build time)
window.__WELCOME_LOCALES__ = ${JSON.stringify(output, null, 2)};
</script>`

// Read the template file and produce the final welcome.html
if (!fs.existsSync(templatePath)) {
  console.error(`Error: Template file not found: ${templatePath}`)
  process.exit(1)
}

let template = fs.readFileSync(templatePath, 'utf8')

// Replace the placeholder
if (template.includes('WELCOME_LOCALES_INLINE_PLACEHOLDER')) {
  template = template.replace('<!-- WELCOME_LOCALES_INLINE_PLACEHOLDER -->', inlineScript)
  fs.writeFileSync(outputPath, template, 'utf8')
  console.log(`Generated welcome.html from template`)
  console.log(`  Languages: ${Object.keys(output).join(', ')}`)
} else {
  console.error(`Error: Placeholder not found in template file`)
  process.exit(1)
}

