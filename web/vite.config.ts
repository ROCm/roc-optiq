import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// In development the API comes from a roc-optiq-server on port 8080. A built
// app is served by the server itself (roc-optiq-server --web web/dist).
// The repo root is allowed so the app can share resources/memory_chart.
export default defineConfig({
  plugins: [react()],
  server: {
    fs: { allow: ['..'] },
    proxy: { '/api': { target: 'http://127.0.0.1:8080', changeOrigin: true } },
  },
})
