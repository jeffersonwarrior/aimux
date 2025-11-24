// v2.2 Task 4: Prettifier UI Controller
// Dashboard UI logic and user interactions for prettifier management
// Provides real-time updates with 10-second auto-refresh

/**
 * PrettifierUI - User interface controller for prettifier dashboard
 * Manages UI state, updates, and user interactions
 */
class PrettifierUI {
    /**
     * Constructor
     * @param {PrettifierAPIClient} apiClient - API client instance
     */
    constructor(apiClient) {
        this.api = apiClient;
        this.elements = {};
        this.refreshInterval = null;
        this.autoRefreshEnabled = true;
        this.lastUpdate = null;
    }

    /**
     * Initialize UI - cache DOM elements and attach event listeners
     * @returns {Promise<void>}
     */
    async init() {
        try {
            // Cache DOM elements
            this.cacheElements();

            // Verify all required elements exist
            if (!this.verifyElements()) {
                console.error('Failed to initialize: Missing required DOM elements');
                this.showMessage('UI initialization failed: Missing DOM elements', 'error');
                return;
            }

            // Attach event listeners
            this.attachEventListeners();

            // Initial load
            await this.loadStatus();

            // Start auto-refresh timer (10 seconds)
            this.startAutoRefresh();

            console.log('PrettifierUI initialized successfully');

        } catch (error) {
            console.error('Failed to initialize PrettifierUI:', error);
            this.showMessage(`Initialization failed: ${error.message}`, 'error');
        }
    }

    /**
     * Cache DOM elements for efficient access
     * @private
     */
    cacheElements() {
        this.elements = {
            card: document.getElementById('prettifier-card'),
            status: document.getElementById('prettifier-status'),
            fmtSpeed: document.getElementById('fmt-speed'),
            fmtThroughput: document.getElementById('fmt-throughput'),
            fmtSuccess: document.getElementById('fmt-success'),
            anthropicFormat: document.getElementById('anthropic-format'),
            openaiFormat: document.getElementById('openai-format'),
            cerebrasFormat: document.getElementById('cerebras-format'),
            streamingEnabled: document.getElementById('streaming-enabled'),
            bufferSize: document.getElementById('buffer-size'),
            timeoutMs: document.getElementById('timeout-ms'),
            securityHardening: document.getElementById('security-hardening'),
            applyBtn: document.getElementById('apply-config'),
            resetBtn: document.getElementById('reset-config'),
            refreshBtn: document.getElementById('refresh-status'),
            message: document.getElementById('config-message')
        };
    }

    /**
     * Verify that all required DOM elements exist
     * @private
     * @returns {boolean} True if all elements found
     */
    verifyElements() {
        const requiredElements = ['card', 'status', 'applyBtn', 'resetBtn', 'refreshBtn', 'message'];

        for (const elementName of requiredElements) {
            if (!this.elements[elementName]) {
                console.error(`Required element not found: ${elementName}`);
                return false;
            }
        }

        return true;
    }

    /**
     * Attach event listeners to UI elements
     * @private
     */
    attachEventListeners() {
        // Button click handlers
        this.elements.applyBtn.addEventListener('click', () => this.applyConfig());
        this.elements.resetBtn.addEventListener('click', () => this.loadStatus());
        this.elements.refreshBtn.addEventListener('click', () => this.manualRefresh());

        // Input validation handlers
        if (this.elements.bufferSize) {
            this.elements.bufferSize.addEventListener('change', (e) => {
                this.validateBufferSize(e.target);
            });
        }

        if (this.elements.timeoutMs) {
            this.elements.timeoutMs.addEventListener('change', (e) => {
                this.validateTimeout(e.target);
            });
        }

        console.log('Event listeners attached');
    }

    /**
     * Load status from API and update UI
     * @returns {Promise<void>}
     */
    async loadStatus() {
        try {
            // Show loading state
            this.setLoadingState(true);

            const status = await this.api.getStatus();
            this.updateUI(status);
            this.lastUpdate = new Date();

            // Clear any error messages
            this.hideMessage();

            console.log('Status loaded successfully', status);

        } catch (error) {
            console.error('Failed to load status:', error);
            this.showMessage(`Failed to load status: ${error.message}`, 'error');
        } finally {
            this.setLoadingState(false);
        }
    }

    /**
     * Update UI with status data
     * @private
     * @param {Object} status - Status object from API
     */
    updateUI(status) {
        try {
            // Update status badge
            if (this.elements.status) {
                const statusText = status.enabled ? 'Enabled' : 'Disabled';
                this.elements.status.textContent = statusText;
                this.elements.status.classList.toggle('disabled', !status.enabled);
            }

            // Update metrics (if available)
            if (status.performance_metrics) {
                this.updateMetrics(status.performance_metrics);
            }

            // Update format selections (if available)
            if (status.format_preferences) {
                this.updateFormatPreferences(status.format_preferences);
            }

            // Update configuration (if available)
            if (status.toon_config || status.cache_ttl_minutes !== undefined) {
                this.updateConfiguration(status);
            }

        } catch (error) {
            console.error('Error updating UI:', error);
            this.showMessage(`UI update error: ${error.message}`, 'error');
        }
    }

    /**
     * Update performance metrics display
     * @private
     * @param {Object} metrics - Performance metrics object
     */
    updateMetrics(metrics) {
        if (this.elements.fmtSpeed && metrics.avg_formatting_time_ms !== undefined) {
            this.elements.fmtSpeed.textContent = `${metrics.avg_formatting_time_ms.toFixed(1)}ms avg`;
        }

        if (this.elements.fmtThroughput && metrics.throughput_requests_per_second !== undefined) {
            this.elements.fmtThroughput.textContent = `${metrics.throughput_requests_per_second} req/s`;
        }

        if (this.elements.fmtSuccess && metrics.success_rate_percent !== undefined) {
            this.elements.fmtSuccess.textContent = `${metrics.success_rate_percent}%`;
        }
    }

    /**
     * Update format preference dropdowns
     * @private
     * @param {Object} formatPrefs - Format preferences object
     */
    updateFormatPreferences(formatPrefs) {
        if (this.elements.anthropicFormat && formatPrefs.anthropic) {
            this.elements.anthropicFormat.value = formatPrefs.anthropic.default_format || 'json-tool-use';
        }

        if (this.elements.openaiFormat && formatPrefs.openai) {
            this.elements.openaiFormat.value = formatPrefs.openai.default_format || 'chat-completion';
        }

        if (this.elements.cerebrasFormat && formatPrefs.cerebras) {
            this.elements.cerebrasFormat.value = formatPrefs.cerebras.default_format || 'speed-optimized';
        }
    }

    /**
     * Update configuration inputs
     * @private
     * @param {Object} status - Status object containing configuration
     */
    updateConfiguration(status) {
        if (this.elements.bufferSize && status.max_cache_size !== undefined) {
            this.elements.bufferSize.value = status.max_cache_size;
        }

        if (this.elements.timeoutMs && status.toon_config && status.toon_config.max_content_length !== undefined) {
            // Note: This is a placeholder - actual timeout might come from a different field
            this.elements.timeoutMs.value = 5000; // Default value
        }
    }

    /**
     * Apply configuration changes
     * @returns {Promise<void>}
     */
    async applyConfig() {
        try {
            // Disable button and show loading state
            this.elements.applyBtn.disabled = true;
            this.elements.applyBtn.textContent = 'Applying...';

            // Collect configuration from UI
            const config = this.collectConfiguration();

            // Validate configuration
            if (!this.validateConfiguration(config)) {
                return; // Validation failed, error message already shown
            }

            // Send update request
            const result = await this.api.updateConfig(config);

            if (result.success) {
                this.showMessage('Configuration applied successfully', 'success');

                // Reload status to reflect changes
                setTimeout(() => this.loadStatus(), 500);
            } else {
                this.showMessage(`Configuration error: ${result.error}`, 'error');
            }

        } catch (error) {
            console.error('Failed to apply config:', error);
            const errorMsg = error.details || error.message;
            this.showMessage(`Failed to apply config: ${errorMsg}`, 'error');

        } finally {
            // Re-enable button
            this.elements.applyBtn.disabled = false;
            this.elements.applyBtn.textContent = 'Apply Configuration';
        }
    }

    /**
     * Collect configuration from UI inputs
     * @private
     * @returns {Object} Configuration object
     */
    collectConfiguration() {
        const config = {
            enabled: true
        };

        // Collect format preferences
        if (this.elements.anthropicFormat || this.elements.openaiFormat || this.elements.cerebrasFormat) {
            config.format_preferences = {};

            if (this.elements.anthropicFormat) {
                config.format_preferences.anthropic = this.elements.anthropicFormat.value;
            }
            if (this.elements.openaiFormat) {
                config.format_preferences.openai = this.elements.openaiFormat.value;
            }
            if (this.elements.cerebrasFormat) {
                config.format_preferences.cerebras = this.elements.cerebrasFormat.value;
            }
        }

        // Collect configuration options
        if (this.elements.streamingEnabled) {
            config.streaming_enabled = this.elements.streamingEnabled.checked;
        }

        if (this.elements.bufferSize) {
            config.max_buffer_size_kb = parseInt(this.elements.bufferSize.value);
        }

        if (this.elements.timeoutMs) {
            config.timeout_ms = parseInt(this.elements.timeoutMs.value);
        }

        return config;
    }

    /**
     * Validate configuration before applying
     * @private
     * @param {Object} config - Configuration to validate
     * @returns {boolean} True if valid
     */
    validateConfiguration(config) {
        // Validate buffer size
        if (config.max_buffer_size_kb !== undefined) {
            if (config.max_buffer_size_kb < 256 || config.max_buffer_size_kb > 8192) {
                this.showMessage('Buffer size must be between 256 and 8192 KB', 'error');
                return false;
            }
        }

        // Validate timeout
        if (config.timeout_ms !== undefined) {
            if (config.timeout_ms < 1000 || config.timeout_ms > 60000) {
                this.showMessage('Timeout must be between 1000 and 60000 ms', 'error');
                return false;
            }
        }

        return true;
    }

    /**
     * Validate buffer size input
     * @private
     * @param {HTMLInputElement} input - Input element
     */
    validateBufferSize(input) {
        const value = parseInt(input.value);
        if (value < 256 || value > 8192) {
            input.setCustomValidity('Buffer size must be between 256 and 8192 KB');
        } else {
            input.setCustomValidity('');
        }
    }

    /**
     * Validate timeout input
     * @private
     * @param {HTMLInputElement} input - Input element
     */
    validateTimeout(input) {
        const value = parseInt(input.value);
        if (value < 1000 || value > 60000) {
            input.setCustomValidity('Timeout must be between 1000 and 60000 ms');
        } else {
            input.setCustomValidity('');
        }
    }

    /**
     * Manual refresh triggered by user
     * @returns {Promise<void>}
     */
    async manualRefresh() {
        this.elements.refreshBtn.disabled = true;
        this.elements.refreshBtn.textContent = 'Refreshing...';

        await this.loadStatus();

        setTimeout(() => {
            this.elements.refreshBtn.disabled = false;
            this.elements.refreshBtn.textContent = 'Refresh Status';
        }, 500);
    }

    /**
     * Start auto-refresh timer
     * @private
     */
    startAutoRefresh() {
        if (this.refreshInterval) {
            clearInterval(this.refreshInterval);
        }

        // Auto-refresh every 10 seconds
        this.refreshInterval = setInterval(() => {
            if (this.autoRefreshEnabled) {
                this.loadStatus();
            }
        }, 10000);

        console.log('Auto-refresh started (10 second interval)');
    }

    /**
     * Stop auto-refresh timer
     */
    stopAutoRefresh() {
        if (this.refreshInterval) {
            clearInterval(this.refreshInterval);
            this.refreshInterval = null;
        }
        this.autoRefreshEnabled = false;
        console.log('Auto-refresh stopped');
    }

    /**
     * Show message to user
     * @param {string} text - Message text
     * @param {string} type - Message type ('success' or 'error')
     */
    showMessage(text, type = 'success') {
        if (!this.elements.message) {
            console.warn('Message element not found, logging message:', text);
            return;
        }

        this.elements.message.textContent = text;
        this.elements.message.className = `message-box ${type}`;
        this.elements.message.style.display = 'block';

        // Auto-hide after 5 seconds
        setTimeout(() => {
            this.hideMessage();
        }, 5000);
    }

    /**
     * Hide message box
     */
    hideMessage() {
        if (this.elements.message) {
            this.elements.message.style.display = 'none';
        }
    }

    /**
     * Set loading state for UI
     * @private
     * @param {boolean} loading - Whether UI is in loading state
     */
    setLoadingState(loading) {
        const buttons = [this.elements.applyBtn, this.elements.resetBtn, this.elements.refreshBtn];

        buttons.forEach(btn => {
            if (btn) {
                btn.disabled = loading;
            }
        });

        if (this.elements.card) {
            this.elements.card.style.opacity = loading ? '0.7' : '1';
        }
    }

    /**
     * Cleanup - stop timers and remove event listeners
     */
    cleanup() {
        this.stopAutoRefresh();
        console.log('PrettifierUI cleaned up');
    }
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = PrettifierUI;
}
