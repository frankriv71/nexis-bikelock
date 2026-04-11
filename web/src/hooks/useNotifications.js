import { useEffect, useRef, useState } from 'react'

export function useNotifications(alerts) {
  const [enabled, setEnabled]     = useState(false)
  const [permission, setPermission] = useState(
    typeof Notification !== 'undefined' ? Notification.permission : 'denied'
  )
  // Empty on mount — populated by the effect on first run so pre-existing
  // alerts are never mistaken for new arrivals when Firestore data loads in.
  const seenIds = useRef(new Set())

  async function requestAndEnable() {
    const result = await Notification.requestPermission()
    setPermission(result)
    if (result === 'granted') setEnabled(true)
  }

  function toggle() {
    if (permission === 'denied') return  // browser blocked, can't re-request
    if (permission === 'granted') {
      setEnabled(e => !e)
    } else {
      requestAndEnable()
    }
  }

  // Fire notifications for newly arriving unacknowledged alerts
  useEffect(() => {
    if (!enabled || permission !== 'granted') {
      seenIds.current = new Set(alerts.map(a => a.id))
      return
    }
    alerts.forEach(alert => {
      if (alert.acknowledged || seenIds.current.has(alert.id)) return
      new Notification('NEXIS — Theft Alert', {
        body: `${alert.type.replace('_', ' ')} · ${alert.distFromAnchor?.toFixed(0) ?? '?'}m from anchor`,
        tag:  alert.id,   // prevents duplicates if multiple tabs open
      })
    })
    seenIds.current = new Set(alerts.map(a => a.id))
  }, [alerts, enabled, permission])

  return { enabled, permission, toggle }
}
