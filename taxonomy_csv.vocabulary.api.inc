<?php

/**
 * @file
 * Prepare and manage vocabularies.
 */

/**
 * Creates vocabulary by its name and returns vocabulary object.
 *
 * @param $name
 *   (Optional) Name of vocabulary to create.
 *
 * @return
 *   Created vocabulary object.
 */
function taxonomy_csv_vocabulary_create($name = '') {
  $name = _taxonomy_csv_vocabulary_name_create($name);

  // Create an empty vocabulary with default Backdrop 7 fields.
  // Hierarchy is updated later.
  $vocabulary = new TaxonomyVocabulary(array(
    'name' => $name,
    'description' => t('Vocabulary created automatically by Taxonomy csv import/export module'),
    'machine_name' => taxonomy_csv_vocabulary_machine_name_create($name),
    'hierarchy' => 2,
    'weight' => 0,
  ));

  $result = taxonomy_vocabulary_save($vocabulary);

  return $vocabulary;
}

/**
 * Helper to create an unused vocabulary name from a string.
 */
function _taxonomy_csv_vocabulary_name_create($name = '') {
  $name = preg_replace('/.csv$/', '', trim(basename(strval($name))));
  $name = (backdrop_strlen($name) == 0) ? t('Auto created vocabulary') : backdrop_substr($name, 0, 250);

  // Invent a unused vocabulary name.
  if (taxonomy_csv_vocabulary_name_check($name)) {
    for (
      $i = 2;
      (taxonomy_csv_vocabulary_name_check("$name $i"));
      $i++) {
    }
    $name = "$name $i";
  }

  return $name;
}

/**
 * Creates a machine name from a string.
 *
 * The name is created by replacing non alphanumeric character by an underscore.
 * Machine name is defined as first 16 cleaned characters of name and a random
 * five characters serial. Fields module prepends 'taxonomy_' to name and check
 * if total lenght is 21 characters max.
 *
 * @param $name
 *   The string to process.
 *
 * @return
 *   The processed string.
 */
function taxonomy_csv_vocabulary_machine_name_create($name) {
  $machine_name = 'vocab_' . uniqid();

  if (taxonomy_csv_vocabulary_machine_name_check($machine_name)) {
    for (
      $i = 2;
      (taxonomy_csv_vocabulary_machine_name_check("$machine_name_$i"));
      $i++) {
    }
    $machine_name = "$machine_name_$i";
  }

  return backdrop_substr($machine_name, 0, 21);
}

/**
 * Checks if a name is a vocabulary machine_name.
 */
function taxonomy_csv_vocabulary_machine_name_check($name) {
  return (taxonomy_vocabulary_load($name) != FALSE);
}

/**
 * Check if a name is a vocabulary name.
 */
function taxonomy_csv_vocabulary_name_check($name) {
  $vocabularies = taxonomy_vocabulary_load_multiple(FALSE);
  foreach ($vocabularies as $vocabulary) {
    if ($vocabulary->name == $name) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
 * Return an array of all term ids of a given vocabulary.
 *
 * @param $machine_name
 *   The vocabulary id from where to fetch term ids.
 *
 * @return
 *   Array of term ids.
 */
function taxonomy_csv_vocabulary_get_tids($machine_name) {
  // Tids are available in backdrop_static('taxonomy_get_tree:terms'), but we
  // prefer to use an entity query to avoid issue with cache, if not updated.
  $query = new EntityFieldQuery;
  $query
    ->entityCondition('entity_type', 'taxonomy_term')
    ->propertyCondition('machine_name', $machine_name)
  ;
  $result = $query->execute();

  return (isset($result['taxonomy_term'])) ?
    array_keys($result['taxonomy_term']) :
    array();
}

/**
 * Return an array of all full terms of a given vocabulary.
 *
 * @param $machine_name
 *   The vocabulary id from where to fetch term ids.
 *
 * @return
 *   Array of full term.
 */
function taxonomy_csv_vocabulary_get_terms($machine_name) {
  $result = taxonomy_csv_vocabulary_get_tids($machine_name);
  return taxonomy_term_load_multiple($result);
}

/**
 * Calculate number of terms in a vocabulary or in all vocabularies.
 *
 * @param $vocabulary_id
 *   (Optional) Id or array of ids of the chosen vocabularies. If not specified,
 *   count terms in all vocabularies.
 *
 * @return
 *   Number of terms in specified vocabularies or in all vocabularies.
 */
function taxonomy_csv_vocabulary_count_terms($vocabulary_id = 0) {
  if (!is_array($vocabulary_id)) {
    $vocabulary_id = array($vocabulary_id);
  }

  $sql = "
    SELECT COUNT(*)
    FROM {taxonomy_term_data}
  ";

  $args = array();
  if (($vocabulary_id != array(0)) && ($vocabulary_id != array('0'))) {
    $sql .= ' WHERE vocabulary IN (:vocabulary) ';
    $args[':vocabulary'] = $vocabulary_id;
  }

  $result = db_query($sql, $args)->fetchField();

  return $result;
}

/**
 * Add or create a field to attach to a vocabulary.
 *
 * @param $vocabulary
 *   Vocabulary machine_name.
 * @param $field
 *   Field array to attach.
 *
 * @return
 *   TRUE if success, FALSE else.
 */
function taxonomy_csv_vocabulary_field_attach($vocabulary, $field) {
  // Check if vocabulary exist.
  $vocabulary = taxonomy_vocabulary_load($vocabulary);
  if ($vocabulary) {
    // Check if field exists in order to create or to update it.
    // No other check is made.
    $prior_field = field_info_field($field['field_name']);

    // The field doesn't exist, so creates it.
    if (empty($prior_field)) {
      $field = field_create_field($field);
    }
    // The field exists: check if an update is needed. Update is needed only
    // with taxonomy_term_reference, because this type requires to use specific
    // vocabularies.
    elseif ($field['type'] == 'taxonomy_term_reference') {
      $flag = FALSE;
      foreach ($prior_field['settings']['allowed_values'] as $allowed_values) {
        // Don't add new allowed values if they exist already.
        if ($allowed_values == $field['settings']['allowed_values'][0]) {
          $flag = TRUE;
          break;
        }
      }
      if (!$flag) {
        $prior_field['settings']['allowed_values'][] = $field['settings']['allowed_values'][0];
        $result = field_update_field($prior_field);
      }
      $field = field_info_field($field['field_name']);
    }
    // The field exists and doesn't need to be updated.
    // Field is already created, so use it.
    else {
      $field = $prior_field;
    }

    // Check if field is already instanced to vocabulary so attach it if needed.
    $prior_instance = field_info_instance('taxonomy_term', $field['field_name'], $vocabulary->machine_name);
    if (empty($prior_instance)) {
      $result = field_create_instance(array(
        'field_name' => $field['field_name'],
        'entity_type' => 'taxonomy_term',
        'bundle' => $vocabulary->machine_name,
        'label' => (isset($field['label']) ? $field['label'] : $field['field_name']),
        'description' => (isset($field['description']) ? $field['description'] : ''),
      ));
    }
    return TRUE;
  }
  return FALSE;
}
