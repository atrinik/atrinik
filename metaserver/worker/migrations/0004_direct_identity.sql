-- Invalidate the old hostname-owned direct rows before identity ownership.
UPDATE servers
   SET server_id = NULL,
       is_public = 0,
       rendezvous_token_hash = NULL
 WHERE connectivity_mode <> 'legacy_tcp';
