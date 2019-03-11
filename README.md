# dconnect-reward

This contract handles the bounty / reward system for dConnect


//lock some of your tokens for a day, returning it with 0.9% interest for yourself, and 0.1% issued to a beneficiary.
cleos -u https://dconnect.live push action <contract> reward '["<user>", "<beneficiary>", "1.0000 <token>", "<memo>", "0"]' -p <user>@active

//resign some of your reward tokens, claiming some of the bounty.
cleos -u https://dconnect.live push action <contract> retire '["<user>", "1.0000 <token>", "<memo>"]' -p <user>@active
