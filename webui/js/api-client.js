// v2.2 Task 3: JavaScript API Client for Prettifier
// Encapsulated HTTP client with error handling for prettifier API endpoints
// Provides getStatus() and updateConfig() methods with timeout handling

/**
 * PrettifierAPIClient - HTTP client for prettifier API
 * Handles all communication with the backend prettifier endpoints
 */
class PrettifierAPIClient {
    /**
     * Constructor
     * @param {string} baseUrl - Base URL for API (default: '/api')
     * @param {number} timeout - Request timeout in milliseconds (default: 5000)
     */
    constructor(baseUrl = '/api', timeout = 5000) {
        this.baseUrl = baseUrl;
        this.timeout = timeout;
    }

    /**
     * Fetch current prettifier status
     * @returns {Promise<Object>} Status object containing configuration and metrics
     * @throws {Error} If request fails or times out
     */
    async getStatus() {
        try {
            const controller = new AbortController();
            const timeoutId = setTimeout(() => controller.abort(), this.timeout);

            const response = await fetch(`${this.baseUrl}/prettifier/status`, {
                method: 'GET',
                headers: {
                    'Content-Type': 'application/json'
                },
                signal: controller.signal
            });

            clearTimeout(timeoutId);

            if (!response.ok) {
                const errorData = await response.json().catch(() => ({}));
                const error = new Error(errorData.error || `HTTP ${response.status}`);
                error.status = response.status;
                error.details = errorData.details || errorData.message;
                throw error;
            }

            return await response.json();

        } catch (error) {
            if (error.name === 'AbortError') {
                const timeoutError = new Error(`Request timeout after ${this.timeout}ms`);
                timeoutError.code = 'TIMEOUT';
                throw timeoutError;
            }

            console.error('Failed to fetch status:', error);
            throw error;
        }
    }

    /**
     * Update prettifier configuration
     * @param {Object} config - Configuration object with prettifier settings
     * @param {boolean} config.enabled - Whether prettifier is enabled
     * @param {string} config.default_prettifier - Default formatter (toon/json/raw)
     * @param {Object} config.format_preferences - Provider-specific format preferences
     * @param {boolean} config.streaming_enabled - Whether streaming is enabled
     * @param {number} config.max_buffer_size_kb - Maximum buffer size in KB
     * @param {number} config.timeout_ms - Timeout in milliseconds
     * @returns {Promise<Object>} Response object with success status and applied config
     * @throws {Error} If request fails, validation fails, or times out
     */
    async updateConfig(config) {
        try {
            // Validate config before sending
            this._validateConfig(config);

            const controller = new AbortController();
            const timeoutId = setTimeout(() => controller.abort(), this.timeout);

            const response = await fetch(`${this.baseUrl}/prettifier/config`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(config),
                signal: controller.signal
            });

            clearTimeout(timeoutId);

            const data = await response.json();

            if (!response.ok) {
                const error = new Error(data.error || `HTTP ${response.status}`);
                error.status = response.status;
                error.details = data.details;
                error.invalid_field = data.details?.invalid_field;
                throw error;
            }

            return data;

        } catch (error) {
            if (error.name === 'AbortError') {
                const timeoutError = new Error(`Request timeout after ${this.timeout}ms`);
                timeoutError.code = 'TIMEOUT';
                throw timeoutError;
            }

            console.error('Failed to update config:', error);
            throw error;
        }
    }

    /**
     * Validate configuration object before sending
     * @private
     * @param {Object} config - Configuration to validate
     * @throws {Error} If configuration is invalid
     */
    _validateConfig(config) {
        if (!config || typeof config !== 'object') {
            throw new Error('Configuration must be an object');
        }

        // Validate default_prettifier if provided
        if (config.default_prettifier !== undefined) {
            const validFormats = ['toon', 'json', 'raw'];
            if (!validFormats.includes(config.default_prettifier)) {
                throw new Error(`Invalid default_prettifier: ${config.default_prettifier}. Must be one of: ${validFormats.join(', ')}`);
            }
        }

        // Validate format_preferences if provided
        if (config.format_preferences !== undefined) {
            if (typeof config.format_preferences !== 'object') {
                throw new Error('format_preferences must be an object');
            }

            const validProviders = ['anthropic', 'openai', 'cerebras'];
            for (const provider of Object.keys(config.format_preferences)) {
                if (!validProviders.includes(provider)) {
                    throw new Error(`Invalid provider: ${provider}. Must be one of: ${validProviders.join(', ')}`);
                }
            }
        }

        // Validate max_buffer_size_kb if provided
        if (config.max_buffer_size_kb !== undefined) {
            if (typeof config.max_buffer_size_kb !== 'number' || config.max_buffer_size_kb < 256 || config.max_buffer_size_kb > 8192) {
                throw new Error('max_buffer_size_kb must be a number between 256 and 8192');
            }
        }

        // Validate timeout_ms if provided
        if (config.timeout_ms !== undefined) {
            if (typeof config.timeout_ms !== 'number' || config.timeout_ms < 1000 || config.timeout_ms > 60000) {
                throw new Error('timeout_ms must be a number between 1000 and 60000');
            }
        }

        // Validate boolean fields
        const booleanFields = ['enabled', 'streaming_enabled'];
        for (const field of booleanFields) {
            if (config[field] !== undefined && typeof config[field] !== 'boolean') {
                throw new Error(`${field} must be a boolean`);
            }
        }
    }

    /**
     * Set timeout for requests
     * @param {number} timeout - Timeout in milliseconds
     */
    setTimeout(timeout) {
        if (typeof timeout !== 'number' || timeout <= 0) {
            throw new Error('Timeout must be a positive number');
        }
        this.timeout = timeout;
    }

    /**
     * Get current timeout setting
     * @returns {number} Current timeout in milliseconds
     */
    getTimeout() {
        return this.timeout;
    }

    /**
     * Set base URL for API
     * @param {string} baseUrl - Base URL for API
     */
    setBaseUrl(baseUrl) {
        if (typeof baseUrl !== 'string') {
            throw new Error('Base URL must be a string');
        }
        this.baseUrl = baseUrl;
    }

    /**
     * Get current base URL
     * @returns {string} Current base URL
     */
    getBaseUrl() {
        return this.baseUrl;
    }

    /**
     * Test connectivity to the API
     * @returns {Promise<boolean>} True if API is reachable
     */
    async testConnectivity() {
        try {
            await this.getStatus();
            return true;
        } catch (error) {
            return false;
        }
    }
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = PrettifierAPIClient;
}
