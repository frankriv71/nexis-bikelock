import { initializeApp } from 'firebase/app'
import { getFirestore } from 'firebase/firestore'

// ─────────────────────────────────────────────────────────────
//  DEMO MODE
//  true  → use mock data, no Firebase connection needed
//  false → live Firestore (current)
// ─────────────────────────────────────────────────────────────
export const DEMO_MODE = false

const firebaseConfig = {
  apiKey:            "AIzaSyAjxHkdVZepyxNCVvj5MnALPIJ9bH01wWQ",
  authDomain:        "nexis-bike-lock.firebaseapp.com",
  projectId:         "nexis-bike-lock",
  storageBucket:     "nexis-bike-lock.firebasestorage.app",
  messagingSenderId: "303748770080",
  appId:             "1:303748770080:web:270c285742590a7fd44f45",
}

const app = initializeApp(firebaseConfig)
export const db = getFirestore(app)
