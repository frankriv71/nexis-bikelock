/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{js,jsx}'],
  theme: {
    extend: {
      colors: {
        bg:          '#0e0d0b',
        surface:     '#161412',
        'surface-2': '#1e1b18',
        border:      '#2a2622',
        amber:       '#c9913a',   // warm gold — brand + ARMING
        cyan:        '#5e9fad',   // muted teal — data/IP
        green:       '#5f9e8f',   // desaturated teal-green — ARMED
        orange:      '#b87040',   // muted orange — WARNING / TAMPER
        red:         '#b05252',   // muted rose-red — ALERT
        text:        '#e2ddd4',   // warm off-white
        muted:       '#706860',   // warm stone
      },
      fontFamily: {
        display: ['Instrument Sans', 'sans-serif'],
        mono:    ['DM Mono', 'monospace'],
      },
      keyframes: {
        'fade-up': {
          from: { opacity: '0', transform: 'translateY(10px)' },
          to:   { opacity: '1', transform: 'translateY(0)' },
        },
        'pulse-ring': {
          '0%':   { boxShadow: '0 0 0 0 currentColor' },
          '70%':  { boxShadow: '0 0 0 8px transparent' },
          '100%': { boxShadow: '0 0 0 0 transparent' },
        },
      },
      animation: {
        'fade-up': 'fade-up 0.4s ease both',
      },
    },
  },
  plugins: [],
}
