import { useState, useEffect } from 'react'
import { db, DEMO_MODE } from '../firebase'
import { collection, query, where, orderBy, onSnapshot, updateDoc, doc } from 'firebase/firestore'

const MOCK_ALERTS = [
  {
    id:             'alert_002',
    lockId:         'nexis_001',
    type:           'TAMPERING',
    timestamp:       new Date(Date.now() - 8 * 60 * 1000).toISOString(),
    lat:             43.038210,
    lng:            -76.134490,
    distFromAnchor:  17.3,
    motionEvents:    3,
    acknowledged:    false,
  },
  {
    id:             'alert_001',
    lockId:         'nexis_001',
    type:           'GEOFENCE_BREACH',
    timestamp:       new Date(Date.now() - 42 * 60 * 1000).toISOString(),
    lat:             43.038950,
    lng:            -76.134200,
    distFromAnchor:  95.1,
    motionEvents:    0,
    acknowledged:    true,
  },
]

const PAGE_SIZE = 50

export function useAlerts(lockId) {
  // Demo mode: start with mock data. Production: start empty, populated by Firestore.
  const [allAlerts, setAllAlerts] = useState(DEMO_MODE ? MOCK_ALERTS : [])
  const [visibleCount, setVisibleCount] = useState(PAGE_SIZE)

  useEffect(() => {
    if (DEMO_MODE) return

    const q = query(
      collection(db, 'alerts'),
      where('lockId', '==', lockId),
      orderBy('timestamp', 'desc'),
    )
    const unsub = onSnapshot(q, (snap) => {
      setAllAlerts(snap.docs.map(d => ({ id: d.id, ...d.data() })))
    })
    return unsub
  }, [lockId])

  async function acknowledgeAlert(alertId) {
    if (DEMO_MODE) {
      setAllAlerts(prev =>
        prev.map(a => a.id === alertId ? { ...a, acknowledged: true } : a)
      )
      return
    }
    await updateDoc(doc(db, 'alerts', alertId), { acknowledged: true })
  }

  const alerts = allAlerts.slice(0, visibleCount)
  const hasMore = allAlerts.length > visibleCount

  function loadMore() {
    setVisibleCount(n => n + PAGE_SIZE)
  }

  return { alerts, acknowledgeAlert, hasMore, loadMore, totalCount: allAlerts.length }
}
