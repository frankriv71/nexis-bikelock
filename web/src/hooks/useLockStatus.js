import { useState, useEffect } from 'react'
import { db, DEMO_MODE } from '../firebase'
import { doc, onSnapshot, updateDoc } from 'firebase/firestore'

const MOCK_STATUS = {
  state:          'ARMED',
  lat:             43.038100,
  lng:            -76.134700,
  anchorLat:       43.038100,
  anchorLng:      -76.134700,
  geofenceRadius:  50,
  lastSeen:        new Date().toISOString(),
  wifiSSID:       'AirOrangeX',
  ip:             '10.1.225.34',
}

export function useLockStatus(lockId) {
  // Demo mode: start with mock data. Production: start null, populated by Firestore.
  const [lockStatus, setLockStatus] = useState(DEMO_MODE ? MOCK_STATUS : null)

  useEffect(() => {
    if (DEMO_MODE) return

    const ref = doc(db, 'locks', lockId)
    const unsub = onSnapshot(ref, (snap) => {
      if (snap.exists()) setLockStatus(snap.data())
    })
    return unsub
  }, [lockId])

  async function setRemoteState(newState) {
    if (DEMO_MODE) {
      setLockStatus(prev => ({ ...prev, state: newState }))
      return
    }
    // ESP32 polls this field on its next Firestore check interval
    await updateDoc(doc(db, 'locks', lockId), { state: newState })
  }

  return { lockStatus, setRemoteState }
}
